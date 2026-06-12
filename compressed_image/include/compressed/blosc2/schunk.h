#pragma once

#include <span>
#include <vector>
#include <cstddef>
#include <cassert>

#include "compressed/macros.h"
#include "compressed/util.h"
#include "compressed/blosc2/util.h"
#include "compressed/detail/scoped_timer.h"
#include "wrapper.h"
#include "schunk_mixin.h"

#include "compressed/cuda/compression.h"

namespace
NAMESPACE_COMPRESSED_IMAGE
{
    namespace detail
    {
        template <typename T>
        struct schunk final :
            public detail::schunk_mixin<T>
        {
            using detail::schunk_mixin<T>::gpu_container;
            using detail::schunk_mixin<T>::cpu_container;

            using detail::schunk_mixin<T>::chunk_bytes;
            using detail::schunk_mixin<T>::chunk;
            using detail::schunk_mixin<T>::is_gpu_chunk;
            using detail::schunk_mixin<T>::to_uncompressed;

            schunk() = default;

            schunk(schunk&& other) noexcept
            {
                this->m_chunks = std::move(other.m_chunks);
                this->m_chunk_size = other.m_chunk_size;
                this->m_block_size = other.m_block_size;
            }

            schunk& operator=(schunk&& other) noexcept
            {
                if (this != &other)
                {
                    this->m_chunks = std::move(other.m_chunks);
                    this->m_chunk_size = other.m_chunk_size;
                    this->m_block_size = other.m_block_size;
                }
                return *this;
            }

            schunk(const schunk& other) = default;
            schunk& operator=(const schunk& other) = default;


            /// Initialize an empty schunk with just a schunk size. The data can then later
            /// be filled with append_chunk for example.
            schunk(size_t block_size, size_t chunk_size)
            {
                util::validate_chunk_size<T>(chunk_size, "schunk");
                this->m_chunk_size = chunk_size;
                this->m_block_size = block_size;
            }

            /// Initialize a super-chunk from the given vector, compressing it
            ///
            /// \param data The data to store
            /// \param block_size The requested block size. It is up to the caller to ensure
            ///                   this is appropriately sized
            /// \param chunk_size The requested chunk size. It is up to the caller to ensure
            ///                   this is appropriately sized (i.e., by using util::align_chunk_to_scanlines)
            /// \param compression_ctx The compression context to be used for compressing the data. Depending on which
            ///                        type this is, this will initialize the data using gpu/cpu compression internally.
            schunk(std::span<const T> data,
                   size_t block_size,
                   size_t chunk_size,
                   compression_context_var compression_ctx)
            {
                util::validate_chunk_size<T>(chunk_size, "schunk");
                this->m_block_size = block_size;
                this->m_chunk_size = chunk_size;

                const size_t num_elements = data.size();
                const size_t num_bytes = num_elements * sizeof(T);

                // Calculate all 'full' chunks and the final remainder (if any).
                const size_t num_full_chunks = num_bytes / this->chunk_bytes();
                const size_t remainder_bytes = num_bytes - (this->chunk_bytes() * num_full_chunks);

                // When compressing using gpu compression, we don't allocate a scratch buffer on the cpu as we internally
                // use a memory-pool on the gpu that we reuse between compressions, making allocations quite cheap.
                if (std::holds_alternative<gpu_compression_context>(compression_ctx))
                {
                    size_t data_offset = 0;

                    for ([[maybe_unused]] auto idx : std::views::iota(size_t{0}, num_full_chunks))
                    {
                        auto subspan = std::span<const T>(data.data() + data_offset, this->chunk_elements());
                        this->append_chunk(std::get<gpu_compression_context>(compression_ctx).ctx, subspan);

                        data_offset += this->chunk_elements();
                    }
                    if (remainder_bytes > 0)
                    {
                        auto subspan = std::span<const T>(data.data() + data_offset, data.size() - data_offset);

                        this->append_chunk(std::get<gpu_compression_context>(compression_ctx).ctx, subspan);
                        // no need to move over the data_offset.
                    }
                }
                else
                {
                    // Compression buffer we will continuously overwrite in our compression, the chunk data is then copied out
                    // of this on initialization.
                    util::default_init_vector<std::byte> compression_buffer(blosc2::min_compressed_size(chunk_size));
                    auto compression_span = std::span<std::byte>(compression_buffer);

                    size_t data_offset = 0;
                    // Initialize the chunks by compressing them.
                    for ([[maybe_unused]] auto idx : std::views::iota(size_t{0}, num_full_chunks))
                    {
                        auto subspan = std::span<const T>(data.data() + data_offset, this->chunk_elements());
                        auto csize = blosc2::compress<T>(
                            std::get<cpu_compression_context>(compression_ctx).compression_ctx.get(),
                            subspan,
                            compression_span
                        );

                        // copy over a new vector containing all the elements from the compression span.
                        this->m_chunks.push_back(
                            util::default_init_vector<std::byte>(
                                compression_span.begin(),
                                compression_span.begin() + csize
                            )
                        );

                        data_offset += this->chunk_elements();
                    }
                    if (remainder_bytes > 0)
                    {
                        auto subspan = std::span<const T>(data.data() + data_offset, data.size() - data_offset);
                        auto csize = blosc2::compress<T>(
                            std::get<cpu_compression_context>(compression_ctx).compression_ctx.get(),
                            subspan,
                            compression_span
                        );

                        // copy over a new vector containing all the elements from the compression span.
                        this->m_chunks.push_back(
                            util::default_init_vector<std::byte>(
                                compression_span.begin(),
                                compression_span.begin() + csize
                            )
                        );

                        // no need to move over the data_offset.
                    }
                }
            }


            void chunk(std::span<T> buffer, size_t index) const override
            {
                this->validate_chunk_index(index);
                if (!this->is_gpu_chunk(index))
                {
                    throw std::runtime_error(
                        "Invalid function overload called for schunk::chunk. The given chunk is not a gpu"
                        " chunk but a cpu chunk."
                    );
                }
                const auto& chunk_data = std::get<gpu_container>(this->m_chunks.at(index));
                auto compressor = cuda::make_compressor<T>(chunk_data);
                std::visit(
                    [&](auto& _compressor)
                    {
                        _compressor.decompress(chunk_data, std::span<T>(buffer));
                    },
                    compressor
                );
            }

            void chunk(blosc2::context_raw_ptr decompression_ctx, std::span<T> buffer, size_t index) const override
            {
                this->validate_chunk_index(index);
                if (this->is_gpu_chunk(index))
                {
                    throw std::runtime_error(
                        "Invalid function overload called for schunk::chunk. The given chunk is not a cpu"
                        " chunk but a gpu chunk."
                    );
                }

                if (buffer.size() < this->chunk_elements(index))
                {
                    throw std::invalid_argument(
                        std::format(
                            "Unable to decompress chunk at idx {} into buffer as the buffer needs to at least have the size {:L}."
                            " Instead got {:L}",
                            index,
                            this->chunk_elements(index),
                            buffer.size()
                        )
                    );
                }

                const auto& chunk_data = std::get<cpu_container>(this->m_chunks.at(index));
                auto chunk_span = std::span<const std::byte>(chunk_data.begin(), chunk_data.end());
                blosc2::decompress(decompression_ctx, std::span<T>(buffer), chunk_span);
            }

            void set_chunk(blosc2::context_ptr& compression_ctx,
                           std::span<T> uncompressed,
                           size_t index,
                           bool validate_chunk_sizes = true) override
            {
                this->validate_chunk_index(index);

                auto compressed = blosc2::compress_to_chunk<T>(compression_ctx, uncompressed);

                // copy over a new vector containing all the elements from the compression span.
                this->m_chunks[index] = std::move(compressed);
                if (validate_chunk_sizes)
                {
                    this->validate_chunk_sizes();
                }
            }

            void set_chunk(cuda::nvcomp_context compression_ctx,
                           std::span<T> uncompressed,
                           size_t index,
                           bool validate_chunk_sizes = true) override
            {
                this->validate_chunk_index(index);

                auto compressor = cuda::make_compressor<T>(compression_ctx.codec);
                cuda::compressed_chunk<T> _chunk{};
                std::visit(
                    [&](auto& _compressor)
                    {
                        _chunk = _compressor.compress(uncompressed, compression_ctx);
                    },
                    compressor
                );

                this->m_chunks[index] = std::move(_chunk);
                if (validate_chunk_sizes)
                {
                    this->validate_chunk_sizes();
                }
            }

            void set_chunk(compression_context_var compression_ctx,
                           std::span<T> uncompressed,
                           size_t index,
                           bool validate_chunk_sizes = true) override
            {
                this->validate_chunk_index(index);

                if (std::holds_alternative<cpu_compression_context>(compression_ctx))
                {
                    auto compressed = blosc2::compress_to_chunk<T>(
                        std::get<cpu_compression_context>(compression_ctx).compression_ctx,
                        uncompressed
                    );
                    this->m_chunks[index] = std::move(compressed);
                }
                else
                {
                    auto compressor = cuda::make_compressor<T>(
                        std::get<gpu_compression_context>(compression_ctx).ctx.codec
                    );
                    cuda::compressed_chunk<T> _chunk{};
                    std::visit(
                        [&](auto& _compressor)
                        {
                            _chunk = _compressor.compress(
                                uncompressed,
                                std::get<gpu_compression_context>(compression_ctx).ctx
                            );
                        },
                        compressor
                    );

                    this->m_chunks[index] = std::move(_chunk);
                    if (validate_chunk_sizes)
                    {
                        this->validate_chunk_sizes();
                    }
                }
            }

            void append_chunk(blosc2::context_ptr& compression_ctx,
                              std::span<T> uncompressed,
                              std::span<std::byte> compression_buff,
                              bool validate_chunk_sizes = true) override
            {
                if (compression_buff.size() < blosc2::min_compressed_size(this->chunk_bytes()))
                {
                    throw std::runtime_error(
                        std::format(
                            "Error while appending chunk to super-chunk. Expected compression buffer to be at least"
                            " {:L} bytes but instead we got {:L} bytes",
                            blosc2::min_compressed_size(this->chunk_bytes()),
                            compression_buff.size()
                        )
                    );
                }
                auto csize = blosc2::compress<T>(compression_ctx, uncompressed, compression_buff);
                assert(csize <= compression_buff.size());
                // copy over a new vector containing all the elements from the compression span.
                this->m_chunks.push_back(cpu_chunk(compression_buff.begin(), compression_buff.begin() + csize));
                if (validate_chunk_sizes)
                {
                    this->validate_chunk_sizes();
                }
            }

            void append_chunk(cuda::nvcomp_context compression_ctx,
                              std::span<const T> uncompressed,
                              bool validate_chunk_sizes = true) override
            {
                auto compressor = cuda::make_compressor<T>(compression_ctx.codec);

                cuda::compressed_chunk<T> _chunk{};
                std::visit(
                    [&](auto& _compressor)
                    {
                        _chunk = _compressor.compress(uncompressed, compression_ctx);
                    },
                    compressor
                );

                this->m_chunks.push_back(std::move(_chunk));
                if (validate_chunk_sizes)
                {
                    this->validate_chunk_sizes();
                }
            }

            void append_chunk(compression_context_var compression_ctx,
                              std::span<T> uncompressed,
                              bool validate_chunk_sizes = true) override
            {
                if (std::holds_alternative<cpu_compression_context>(compression_ctx))
                {
                    auto compressed = blosc2::compress_to_chunk<T>(
                        std::get<cpu_compression_context>(compression_ctx).compression_ctx,
                        uncompressed
                    );
                    this->m_chunks.push_back(std::move(compressed));
                }
                else
                {
                    auto compressor = cuda::make_compressor<T>(
                        std::get<gpu_compression_context>(compression_ctx).ctx.codec
                    );
                    cuda::compressed_chunk<T> _chunk{};
                    std::visit(
                        [&](auto& _compressor)
                        {
                            _chunk = _compressor.compress(
                                uncompressed,
                                std::get<gpu_compression_context>(compression_ctx).ctx
                            );
                        },
                        compressor
                    );

                    this->m_chunks.push_back(std::move(_chunk));
                }
                if (validate_chunk_sizes)
                {
                    this->validate_chunk_sizes();
                }
            };

            size_t chunk_bytes(size_t index) const override
            {
                if (is_gpu_chunk(index))
                {
                    const auto& _chunk = std::get<gpu_container>(this->m_chunks.at(index));
                    return _chunk.byte_size();
                }
                const auto& _chunk = std::get<cpu_container>(this->m_chunks.at(index));
                return blosc2::chunk_num_elements<T>(_chunk) * sizeof(T);
            }

            /// The total compressed size of the schunk
            size_t csize() const noexcept override
            {
                size_t _size = 0;
                size_t index = 0;
                for ([[maybe_unused]] const auto& chunk : this->m_chunks)
                {
                    if (is_gpu_chunk(index))
                    {
                        const auto& _chunk = std::get<gpu_container>(this->m_chunks.at(index));
                        _size += _chunk.csize();
                    }
                    else
                    {
                        const auto& _chunk = std::get<cpu_container>(this->m_chunks.at(index));
                        _size += _chunk.size();
                    }
                    ++index;
                }
                return _size;
            };

            [[nodiscard]] size_t size() const noexcept override
            {
                size_t _size = 0;
                size_t index = 0;
                for ([[maybe_unused]] const auto& chunk : this->m_chunks)
                {
                    if (is_gpu_chunk(index))
                    {
                        const auto& _chunk = std::get<gpu_container>(this->m_chunks.at(index));
                        _size += _chunk.size();
                    }
                    else
                    {
                        const auto& _chunk = std::get<cpu_container>(this->m_chunks.at(index));
                        _size += blosc2::chunk_num_elements<T>(_chunk);
                    }
                    ++index;
                }
                return _size;
            };
        };
    } // detail
} // NAMESPACE_COMPRESSED_IMAGE
