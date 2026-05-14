#pragma once

#include <array>
#include <vector>
#include <span>
#include <memory>
#include <optional>
#include <limits>

#include "blosc2.h"
#include "nlohmann/json.hpp"

#include "macros.h"
#include "enums.h"
#include "blosc2/wrapper.h"
#include "blosc2/typedefs.h"
#include "blosc2/schunk.h"
#include "blosc2/lazyschunk.h"
#include "constants.h"
#include "context.h"
#include "util.h"
#include "detail/scoped_timer.h"
#include "iterators/iterator.h"


namespace
NAMESPACE_COMPRESSED_IMAGE
{
    template <typename T>
    struct channel : public std::ranges::view_interface<channel<T>>
    {
        using value_type = T;
        using iterator = channel_iterator<T>;
        using const_iterator = channel_iterator<const T>;

        channel(channel&& other) noexcept
        {
            m_schunk = std::move(other.m_Schunk);
            m_codec = other.m_Codec;
            m_compression_context = std::move(other.m_CompressionContext);
            m_compression_level = other.m_CompressionLevel;
            m_width = other.m_Width;
            m_height = other.m_Height;
        };

        channel& operator=(channel&& other) noexcept
        {
            if (this != &other)
            {
                m_schunk = std::move(other.m_Schunk);
                m_codec = other.m_Codec;
                m_compression_context = std::move(other.m_CompressionContext);
                m_compression_level = other.m_CompressionLevel;
                m_width = other.m_Width;
                m_height = other.m_Height;
            }
            return *this;
        };
        channel(const channel&) = delete;
        channel& operator=(const channel&) = delete;


        /// Default ctor, ensures the schunk and compression/decompression contexts are always initialized
        /// into valid states. This will not generate a valid channel however, and the ctor taking data or the static
        /// functions `zeros` and `full` are preferred.
        channel()
        {
            m_schunk = std::make_shared<schunk_var<T>>(
                detail::lazy_schunk<T>(0, 1, s_default_blocksize, s_default_chunksize)
            );
            m_compression_context = this->create_compression_context(
                enums::codec::lz4,
                std::thread::hardware_concurrency() / 2,
                5,
                s_default_blocksize,
                -1
            );
        };

        /// Initialize the channel with the given data.
        ///
        /// \param data The span of input data to be compressed.
        /// \param width The width of the image channel.
        /// \param height The height of the image channel.
        /// \param compression_codec The compression codec to be used (default is lz4).
        /// \param compression_level The compression level (default is 5).
        /// \param block_size The size of the blocks stored inside the chunks, defaults to 32KB which is enough to
        ///					  comfortably fit into the L1 cache of most modern CPUs. If you know your cpu can handle
        ///					  larger blocks feel free to up this number although this may not increase performance
        /// \param chunk_size The size of each individual chunk, defaults to 4MB which is enough to hold a 2048x2048 channel.
        ///					  This should be tweaked to be no larger than the size of the usual images you are expecting
        ///					  to compress for optimal performance, but this could be upped which might give better compression
        ///					  ratios. Must be a multiple of sizeof(T).
        /// \param gpu_device The GPU device to user for compression/decompression. This only has an effect if the codec
        ///                   chosen is one of the gpu_* codecs. If not specified, the best default device will be used.
        ///                   To find out which devices are available, we provide the utility functions
        ///                   `NAMESPACE_COMPRESSED_IMAGE::cuda::device_names()` and `NAMESPACE_COMPRESSED_IMAGE::cuda::devices()`.
        ///                   The logical index into the arrays returned by those functions is the index that is passed
        ///                   here.
        channel(
            const std::span<const T> data,
            size_t width,
            size_t height,
            enums::codec compression_codec = enums::codec::lz4,
            uint8_t compression_level = 9,
            size_t block_size = s_default_blocksize,
            size_t chunk_size = s_default_chunksize,
            std::optional<int> gpu_device = std::nullopt
        )
        {
            _COMPRESSED_PROFILE_FUNCTION();
            m_width = width;
            m_height = height;
            m_codec = compression_codec;
            m_compression_level = util::ensure_compression_level(compression_level);
            if (data.size() != width * height)
            {
                throw std::runtime_error(
                    std::format(
                        "Invalid channel data passed. Expected its size to match up to width * height ({} * {}) which would be {:L}."
                        " Instead received {:L}",
                        width,
                        height,
                        width * height,
                        data.size()
                    )
                );
            }

            if (enums::is_gpu_codec(m_codec))
            {
                // Ensure the gpu index passed is valid. We treat this as a failure instead of falling back to some
                // other value as this indicates the user passed an invalid device.
                if (cuda::is_available() && gpu_device && gpu_device.value() > cuda::devices().size())
                {
                    throw std::invalid_argument(
                        std::format(
                            "Invalid GPU device index passed to compressed::channel constructor. Expected a value between 0 and {:L} but instead got {:L}",
                            cuda::devices().size(),
                            gpu_device.value()
                        )
                    );
                }
            }
            else
            {
                // c-blosc2 chunks can at most be 2 gigabytes so the set chunk size should not exceed this.
                assert(chunk_size < std::numeric_limits<int32_t>::max());
                assert(block_size < chunk_size);
            }


            m_compression_context = this->create_compression_context(
                m_codec,
                std::thread::hardware_concurrency() / 2,
                m_compression_level,
                block_size,
                gpu_device.value_or(0)
            );


            // Align the chunks to the scanlines, makes our lifes a lot easier on read / write.
            auto chunk_size_aligned = util::align_chunk_to_scanlines_bytes<T>(m_width, chunk_size);
            m_schunk = std::make_shared<schunk_var<T>>(
                detail::schunk<T>(data, block_size, chunk_size_aligned, m_compression_context)
            );
        }


        /// Initialize the channel with the given data.
        ///
        /// \param schunk The initialized super-chunk.
        /// \param width The width of the image channel.
        /// \param height The height of the image channel.
        /// \param compression_codec The compression codec to be used.
        /// \param compression_level The compression level (default is 5).
        channel(
            schunk_var<T> schunk,
            size_t width,
            size_t height,
            enums::codec compression_codec = enums::codec::lz4,
            uint8_t compression_level = 9
        )
        {
            _COMPRESSED_PROFILE_FUNCTION();
            m_codec = compression_codec;
            m_compression_level = util::ensure_compression_level(compression_level);

            if (std::holds_alternative<detail::schunk<T>>(schunk))
            {
                if (std::get<detail::schunk<T>>(schunk).size() != width * height)
                {
                    throw std::invalid_argument(
                        std::format(
                            "Invalid schunk passed to compressed::channel constructor. Expected a size of {:L} but instead got {:L}",
                            width * height,
                            std::get<detail::schunk<T>>(schunk).size()
                        )
                    );
                }
            }
            else if (std::holds_alternative<detail::lazy_schunk<T>>(schunk))
            {
                if (std::get<detail::lazy_schunk<T>>(schunk).size() != width * height)
                {
                    throw std::invalid_argument(
                        std::format(
                            "Invalid schunk passed to compressed::channel constructor. Expected a size of {:L} but instead got {:L}",
                            width * height,
                            std::get<detail::schunk<T>>(schunk).size()
                        )
                    );
                }
            }

            m_schunk = std::make_shared<schunk_var<T>>(std::move(schunk));
            m_width = width;
            m_height = height;

            // Store the compression and decompression contexts, retrieving the block size from the underlying schunk
            // wrapper
            std::visit(
                [&](auto& _schunk)
                {
                    m_compression_context = this->create_compression_context(
                        m_codec,
                        std::thread::hardware_concurrency() / 2,
                        m_compression_level,
                        _schunk.max_block_size(),
                        0
                    );
                },
                *m_schunk
            );
        }


        /// Create a channel filled with zeros.
        ///
        /// Generates a lazy-channel which only stores a single value T per-chunk, only setting this to a compressed buffer
        /// if set with something like `set_chunk`. This is especially memory efficient and should be the preferred way
        /// when wanting to generate an empty channel only filling out some parts (i.e. sparse cryptomatte loading).
        ///
        /// \param width The width of the image channel.
        /// \param height The height of the image channel.
        /// \param compression_codec The compression codec to be used.
        /// \param compression_level The compression level (default is 9).
        /// \param block_size The size of the blocks stored inside the chunks, defaults to 32KB which is enough to
        ///                   comfortably fit into the L1 cache of most modern CPUs.
        /// \param chunk_size The size of each individual chunk, defaults to 4MB. Should be no larger than the expected image size
        ///                   for optimal performance and must be a multiple of sizeof(T).
        /// \return A channel instance with all values initialized to zero.
        static channel zeros(
            size_t width,
            size_t height,
            enums::codec compression_codec = enums::codec::lz4,
            uint8_t compression_level = 9,
            size_t block_size = s_default_blocksize,
            size_t chunk_size = s_default_chunksize
        )
        {
            return channel<T>::full(
                width,
                height,
                static_cast<T>(0),
                compression_codec,
                compression_level,
                block_size,
                chunk_size
            );
        }

        /// Create a zero-initialized channel with the same shape and compression parameters as another channel.
        ///
        /// Generates a lazy-channel which only stores a single value T per-chunk, only setting this to a compressed buffer
        /// if set with something like `set_chunk`. This is especially memory efficient and should be the preferred way
        /// when wanting to generate an empty channel only filling out some parts (i.e. sparse cryptomatte loading).
        ///
        /// \param other The reference channel from which to copy shape and compression settings.
        /// \return A new channel instance with the same dimensions and compression settings as \p other, filled with zeros.
        static channel zeros_like(const channel& other)
        {
            return channel<T>::zeros(
                other.width(),
                other.height(),
                other.compression(),
                other.compression_level(),
                other.block_size(),
                other.chunk_size()
            );
        }

        /// Create a channel filled with a specific value.
        ///
        /// Generates a lazy-channel which only stores a single value T per-chunk, only setting this to a compressed buffer
        /// if set with something like `set_chunk`. This is especially memory efficient and should be the preferred way
        /// when wanting to generate an empty channel only filling out some parts (i.e. sparse cryptomatte loading).
        ///
        /// \param width The width of the image channel.
        /// \param height The height of the image channel.
        /// \param fill_value The value to fill the channel with.
        /// \param compression_codec The compression codec to be used.
        /// \param compression_level The compression level (default is 9).
        /// \param block_size The size of the blocks stored inside the chunks, defaults to 32KB.
        /// \param chunk_size The size of each individual chunk, defaults to 4MB. Should be no larger than the expected image size
        ///                   for optimal performance and must be a multiple of sizeof(T).
        /// \return A channel instance with all values initialized to \p fill_value.
        static channel full(
            size_t width,
            size_t height,
            T fill_value,
            enums::codec compression_codec = enums::codec::lz4,
            uint8_t compression_level = 9,
            size_t block_size = s_default_blocksize,
            size_t chunk_size = s_default_chunksize
        )
        {
            const size_t chunk_size_aligned = util::align_chunk_to_scanlines_bytes<T>(width, chunk_size);
            const size_t num_elements = width * height;

            auto schunk = detail::lazy_schunk<T>(fill_value, num_elements, block_size, chunk_size_aligned);
            return channel(std::move(schunk), width, height, compression_codec, compression_level);
        }


        /// Create a channel filled with a specific value and the same shape and compression settings as another channel.
        ///
        /// Generates a lazy-channel which only stores a single value T per-chunk, only setting this to a compressed buffer
        /// if set with something like `set_chunk`. This is especially memory efficient and should be the preferred way
        /// when wanting to generate an empty channel only filling out some parts (i.e. sparse cryptomatte loading).
        ///
        /// \param other The reference channel from which to copy shape and compression settings.
        /// \param fill_value The value to fill the channel with.
        /// \return A new channel instance filled with \p fill_value and the same dimensions and compression settings as \p other.
        static channel full_like(const channel& other, T fill_value)
        {
            return channel<T>::full(
                other.width(),
                other.height(),
                fill_value,
                other.compression(),
                other.compression_level(),
                other.block_size(),
                other.chunk_size()
            );
        }

        /// Returns an iterator pointing to the beginning of the compressed data.
        ///
        /// \return An iterator to the beginning of the compressed data.
        iterator begin()
        {
            return iterator(
                m_schunk,
                m_compression_context,
                0,
                m_width,
                m_height
            );
        }

        /// Returns an iterator pointing to the end of the compressed data.
        ///
        /// \return An iterator to the end of the compressed data.
        iterator end()
        {
            if (m_schunk)
            {
                return std::visit(
                    [&](auto& schunk)
                    {
                        return iterator(
                            m_schunk,
                            m_compression_context,
                            schunk.num_chunks(),
                            m_width,
                            m_height
                        );
                    },
                    *m_schunk
                );
            }
            throw std::runtime_error("Internal Error: Unable to create end iterator as m_Schunk is uninitialized.");
        }

        /// Update the number of threads used internally by c-blosc2 for compression and decompression. Only valid for
        /// CPU compression/decompression
        ///
        /// \param nthreads The number of threads to use for compression and decompression.
        /// \param block_size The block size to compress to
        void update_nthreads(size_t nthreads, size_t block_size = s_default_blocksize)
        {
            if (enums::is_gpu_codec(m_codec))
            {
                return;
            }

            m_compression_context = this->create_compression_context(
                m_codec,
                nthreads,
                m_compression_level,
                block_size,
                0
            );
        }

        /// The channel width.
        ///
        /// \return The width of the channel.
        size_t width() const noexcept
        {
            return m_width;
        }

        /// The channel height.
        ///
        /// \return The height of the channel.
        size_t height() const noexcept
        {
            return m_height;
        }

        /// Retrieve the compression codec used.
        ///
        /// \return The compression codec.
        enums::codec compression() const noexcept
        {
            return m_codec;
        }

        /// Retrieve the compression level used.
        ///
        /// \return The compression level (typically from 1-9).
        uint8_t compression_level() const noexcept
        {
            return m_compression_level;
        }

        /// Retrieve the compressed data size.
        ///
        /// \return The size of the compressed data in bytes.
        size_t compressed_bytes() const
        {
            if (!m_schunk)
            {
                throw std::runtime_error(
                    "Channel instance is not properly initialized, unable to get decompressed data"
                );
            }

            if (std::holds_alternative<detail::schunk<T>>(*m_schunk))
            {
                return std::get<detail::schunk<T>>(*m_schunk).csize();
            }
            else if (std::holds_alternative<detail::lazy_schunk<T>>(*m_schunk))
            {
                return std::get<detail::lazy_schunk<T>>(*m_schunk).csize();
            }
            return {};
        }

        /// Retrieve the uncompressed data size.
        ///
        /// \return The size of the uncompressed data in elements.
        size_t uncompressed_size() const
        {
            if (!m_schunk)
            {
                throw std::runtime_error(
                    "Channel instance is not properly initialized, unable to get decompressed data"
                );
            }

            if (std::holds_alternative<detail::schunk<T>>(*m_schunk))
            {
                return std::get<detail::schunk<T>>(*m_schunk).size();
            }
            else if (std::holds_alternative<detail::lazy_schunk<T>>(*m_schunk))
            {
                return std::get<detail::lazy_schunk<T>>(*m_schunk).size();
            }
            return {};
        }

        /// Retrieve the total number of chunks the channel stores.
        ///
        /// \return The number of chunks.
        size_t num_chunks() const
        {
            assert(m_schunk != nullptr);

            if (std::holds_alternative<detail::schunk<T>>(*m_schunk))
            {
                return std::get<detail::schunk<T>>(*m_schunk).num_chunks();
            }
            else if (std::holds_alternative<detail::lazy_schunk<T>>(*m_schunk))
            {
                return std::get<detail::lazy_schunk<T>>(*m_schunk).num_chunks();
            }
            return {};
        }

        /// \brief Retrieve the block size (in bytes) of the channel
        ///
        /// The internal blosc2 implementation reserves changing this value on compression so it may be possible
        /// that this is not the value you initially set.
        ///
        /// \return The block size (in bytes).
        size_t block_size() const
        {
            assert(m_schunk != nullptr);
            return std::visit(
                [&](auto& schunk)
                {
                    return schunk.max_block_size();
                },
                *m_schunk
            );
        }

        /// \brief Retrieve the chunk size (in bytes) of the channel
        ///
        /// This will be all of the chunk sizes except for the last chunk. The last chunk may be smaller so to accurately
        /// capture it you should use the override with a size_t
        ///
        /// \return The chunk size (in bytes).
        size_t chunk_size() const noexcept
        {
            assert(m_schunk != nullptr);
            return std::visit(
                [&](auto& schunk)
                {
                    return schunk.chunk_bytes();
                },
                *m_schunk
            );
        }

        size_t chunk_elems() const
        {
            auto chunk_size = this->chunk_size();
            assert(chunk_size % sizeof(T) == 0);
            return chunk_size / sizeof(T);
        }

        /// \brief Retrieve the chunk size (in bytes) of the channel at the given chunk index.
        ///
        /// \return The chunk size (in bytes) at index `chunk_index`.
        ///
        /// \throws std::out_of_range if the chunk index is invalid
        size_t chunk_size(size_t chunk_index) const
        {
            assert(m_schunk != nullptr);
            return std::visit(
                [&](auto& schunk)
                {
                    return schunk.chunk_bytes(chunk_index);
                },
                *m_schunk
            );
        }

        size_t chunk_elems(size_t chunk_index) const
        {
            auto chunk_size = this->chunk_size(chunk_index);
            assert(chunk_size % sizeof(T) == 0);
            return chunk_size / sizeof(T);
        }


        /// Retrieves and decompresses a chunk of data into the provided buffer.
        ///
        /// This function retrieves the chunk at the given index from the internal `schunk`,
        /// decompresses it using the current decompression context, and stores the result in `buffer`.
        ///
        /// \param buffer A span representing the destination buffer to store the decompressed data.
        ///               Must be large enough to hold one chunk of decompressed data.
        /// \param chunk_idx The index of the chunk to retrieve.
        ///
        /// \throws std::runtime_error if the internal `schunk` pointer is not initialized.
        void get_chunk(std::span<T> buffer, size_t chunk_idx) const
        {
            if (!m_schunk)
            {
                throw std::runtime_error(
                    "Internal Error: Channel instance is not properly initialized, unable to get decompressed data"
                );
            }

            return std::visit(
                [&](const auto& schunk)
                {
                    // We cheat a little bit here by creating this compression ctx on the fly, unfortunately this is
                    // necessary as blosc2 will actually modify the ctx on decompression.
                    auto decomp_ctx = blosc2::create_decompression_context(m_Nthreads);
                    return schunk.chunk(decomp_ctx, buffer, chunk_idx);
                },
                *m_schunk
            );
        }

        /// Compresses and sets a chunk of data from the provided buffer at the specified index.
        ///
        /// This function compresses the data in the provided buffer using the current compression
        /// context and writes it into the internal `schunk` at the given index.
        ///
        /// \param buffer A span representing the source data to be compressed and stored.
        /// \param chunk_idx The index of the chunk to overwrite or set with the compressed data.
        ///
        /// \throws std::runtime_error if the internal `schunk` pointer is not initialized.
        void set_chunk(std::span<T> buffer, size_t chunk_idx)
        {
            if (!m_schunk)
            {
                throw std::runtime_error(
                    "Internal Error: Channel instance is not properly initialized, unable to set data"
                );
            }

            return std::visit(
                [&](auto& schunk)
                {
                    if (buffer.size() != schunk.chunk_elements(chunk_idx))
                    {
                        throw std::invalid_argument(
                            std::format(
                                "Invalid chunk passed to `set_chunk`. Expected this to contain exactly {} elements."
                                " Instead it holds {}. This is likely due to having not correctly checked the number"
                                " of elements.",
                                schunk.chunk_elements(chunk_idx),
                                buffer.size()
                            )
                        );
                    }

                    return schunk.set_chunk(m_compression_context, buffer, chunk_idx);
                },
                *m_schunk
            );
        }

        /// Get the decompressed data as a vector.
        ///
        /// \throws std::runtime_error if the internal `schunk` pointer is not initialized.
        ///
        /// \return A vector containing the decompressed data.
        std::vector<T> get_decompressed() const
        {
            if (!m_schunk)
            {
                throw std::runtime_error(
                    "Internal Error: Channel instance is not properly initialized, unable to get decompressed data"
                );
            }
            return std::visit(
                [&](const auto& schunk)
                {
                    // We cheat a little bit here by creating this compression ctx on the fly, unfortunately this is
                    // necessary as blosc2 will actually modify the ctx on decompression.
                    auto decomp_ctx = blosc2::create_decompression_context(m_Nthreads);
                    return schunk.to_uncompressed(decomp_ctx);
                },
                *m_schunk
            );
        }

        /// Equality operators, compares pointers to check for equality
        bool operator==(const channel<T>& other) const noexcept
        {
            return this == &other;
        }

    private:
        /// The storage for the internal data, stored contiguously in a compressed data format
        schunk_var_ptr<T> m_schunk = nullptr;
        /// The compression/decompression context. Uses either blosc2 or cuda depending on the set compression codec.
        compression_context_var m_compression_context{};
        /// The compression codec in use.
        enums::codec m_codec = enums::codec::lz4;
        /// Compression level.
        uint8_t m_compression_level = 9;

        /// The width and height of the channel.
        size_t m_width = 1;
        size_t m_height = 1;

    private:
        /// \brief Create a compression context for the given codec.
        ///
        /// This will initialize either a gpu or cpu compressor/decompressor, returning it.
        ///
        /// \param codec The compression codec, the type of context to initialize is inferred from this.
        /// \param num_threads The compression/decompression threads. Only used when the codec is cpu-based
        /// \param compression_level The compression level. Only used when the codec is cpu-based
        /// \param block_size The block size for the compressed data.
        /// \param gpu_device The GPU device to use for compression/decompression. Only used when the codec is gpu-based
        static compression_context_var create_compression_context(
            const enums::codec codec,
            const size_t num_threads,
            const size_t compression_level,
            const size_t block_size,
            const int gpu_device
        )
        {
            if (enums::is_gpu_codec(codec))
            {
                return gpu_compression_context{
                    .ctx = cuda::make_compression_context<T>(codec, gpu_device, block_size)

                };
            }
            else
            {
                return cpu_compression_context{
                    .compression_ctx = blosc2::create_decompression_context(num_threads),
                    .decompression_ctx = blosc2::create_compression_context<T>(
                        num_threads,
                        codec,
                        compression_level,
                        block_size
                    ),
                    .nthreads = num_threads
                };
            }
        }
    };
} // NAMESPACE_COMPRESSED_IMAGE
