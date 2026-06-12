#pragma once

#include <span>
#include <vector>
#include <cstddef>
#include <variant>

#include "compressed/macros.h"
#include "compressed/blosc2/util.h"
#include "compressed/util.h"
#include "wrapper.h"
#include "schunk_mixin.h"
#include "compressed/cuda/compression.h"

#include "compressed/detail/scoped_timer.h"

namespace
NAMESPACE_COMPRESSED_IMAGE
{
    namespace detail
    {
        /// Wrapper representing a lazy chunk holding either an initialized (and compressed) chunk
        /// in the form of a byte array or just a single T representing a lazy state
        template <typename T, typename _storage_type>
        struct lazy_chunk
        {
            std::variant<_storage_type, T> value;
            size_t num_elements = 0;

            lazy_chunk(std::variant<_storage_type, T> v, size_t n) noexcept
                : value(std::move(v)), num_elements(n)
            {
            }

            size_t byte_size() const noexcept
            {
                return num_elements * sizeof(T);
            }

            bool is_lazy() const noexcept
            {
                return std::holds_alternative<T>(this->value);
            }
        };


        template <typename T>
        struct lazy_schunk final :
            public detail::schunk_mixin<
                T, /* element type */
                detail::lazy_chunk<T, detail::gpu_chunk<T>>, /* gpu storage type */
                detail::lazy_chunk<T, detail::cpu_chunk> /* cpu storage type */
            >
        {
            /// Bring the gpu_container and cpu_container using declarations into this struct
            using detail::schunk_mixin<
                T,
                detail::lazy_chunk<T, detail::gpu_chunk<T>>,
                detail::lazy_chunk<T, detail::cpu_chunk>
            >::gpu_container;
            using detail::schunk_mixin<
                T,
                detail::lazy_chunk<T, detail::gpu_chunk<T>>,
                detail::lazy_chunk<T, detail::cpu_chunk>
            >::cpu_container;

            using detail::schunk_mixin<
                T,
                detail::lazy_chunk<T, detail::gpu_chunk<T>>,
                detail::lazy_chunk<T, detail::cpu_chunk>
            >::chunk;
            using detail::schunk_mixin<
                T,
                detail::lazy_chunk<T, detail::gpu_chunk<T>>,
                detail::lazy_chunk<T, detail::cpu_chunk>
            >::chunk_bytes;
            using detail::schunk_mixin<
                T,
                detail::lazy_chunk<T, detail::gpu_chunk<T>>,
                detail::lazy_chunk<T, detail::cpu_chunk>
            >::is_gpu_chunk;
            using detail::schunk_mixin<
                T,
                detail::lazy_chunk<T, detail::gpu_chunk<T>>,
                detail::lazy_chunk<T, detail::cpu_chunk>
            >::to_uncompressed;

            lazy_schunk() = default;

            lazy_schunk(lazy_schunk&& other) noexcept
            {
                this->m_chunks = std::move(other.m_chunks);
                this->m_chunk_size = other.m_chunk_size;
                this->m_block_size = other.m_block_size;
            }

            lazy_schunk& operator=(lazy_schunk&& other) noexcept
            {
                if (this != &other)
                {
                    this->m_chunks = std::move(other.m_chunks);
                    this->m_chunk_size = other.m_chunk_size;
                    this->m_block_size = other.m_block_size;
                }
                return *this;
            }

            lazy_schunk(const lazy_schunk& other) = default;
            lazy_schunk& operator=(const lazy_schunk& other) = default;


            /// Initialize a lazy super-chunk from the given value, has a near-zero
            /// cost with the chunks only being initialized on read/modify.
            ///
            /// \param value The initial value to fill.
            /// \param num_elements The size to initialize the data with.
            /// \param block_size The requested chunk size. It is up to the caller to ensure
            ///                   this is appropriately sized
            /// \param chunk_size The requested chunk size. It is up to the caller to ensure
            ///                   this is appropriately sized (i.e. by using util::align_chunk_to_scanlines)
            lazy_schunk(const T value, const size_t num_elements, const size_t block_size, const size_t chunk_size)
            {
                util::validate_chunk_size<T>(chunk_size, "lazy_schunk");
                this->m_block_size = block_size;
                this->m_chunk_size = chunk_size;

                size_t num_bytes = num_elements * sizeof(T);

                // Calculate all 'full' chunks and the final remainder (if any).
                size_t num_full_chunks = num_bytes / this->m_chunk_size;
                size_t remainder_bytes = num_bytes - (this->m_chunk_size * num_full_chunks);

                // Initialize lazy chunks with the provided value of T
                for ([[maybe_unused]] auto idx : std::views::iota(size_t{0}, num_full_chunks))
                {
                    detail::lazy_chunk<T, cpu_chunk> chunk = {value, this->m_chunk_size / sizeof(T)};
                    this->m_chunks.push_back(std::move(chunk));
                }
                if (remainder_bytes > 0)
                {
                    detail::lazy_chunk<T, gpu_chunk<T>> chunk = {value, remainder_bytes / sizeof(T)};
                    this->m_chunks.push_back(std::move(chunk));
                }
            }

            size_t chunk_bytes(size_t index) const override
            {
                if (index > this->m_chunks.size() - 1)
                {
                    throw std::out_of_range(
                        std::format(
                            "Cannot access index {} in lazy-schunk. Total amount of chunks is {}",
                            index,
                            this->m_chunks.size()
                        )
                    );
                }

                return std::visit(
                    [&](const auto& chunk)
                    {
                        return chunk.num_elements * sizeof(T);
                    },
                    this->m_chunks[index]
                );
            }

            /// Generate an uncompressed vector from the chunks, using the decompression context
            /// to perform the decompression.
            std::vector<T> to_uncompressed(
                cpu_compression_context& cpu_ctx,
                gpu_compression_context gpu_ctx
            ) const override
            {
                std::vector<T> uncompressed(this->size(), this->lazy_chunk_value());

                size_t offset = 0; // element offset
                for (const auto& chunk : this->m_chunks)
                {
                    if (std::holds_alternative<gpu_container>(chunk))
                    {
                        const auto& _chunk_val = std::get<gpu_container>(chunk);

                        // Since we already initialized the uncompressed data to the lazy chunks' value we don't need
                        // to do any filling here.
                        if (_chunk_val.is_lazy())
                        {
                            offset += _chunk_val.num_elements;
                            continue;
                        }

                        auto subspan = std::span<T>(uncompressed.data() + offset, _chunk_val.num_elements);

                        auto compressor = cuda::make_compressor<T>(gpu_ctx.ctx.codec);
                        std::visit(
                            [&](auto& _compressor)
                            {
                                _compressor.decompress(std::get<gpu_chunk<T>>(_chunk_val.value), subspan);
                            },
                            compressor
                        );

                        offset += _chunk_val.num_elements;
                    }
                    else
                    {
                        const auto& _chunk_val = std::get<cpu_container>(chunk);

                        // Since we already initialized the uncompressed data to the lazy chunks' value we don't need
                        // to do any filling here.
                        if (_chunk_val.is_lazy())
                        {
                            offset += _chunk_val.num_elements;
                            continue;
                        }

                        auto subspan = std::span<T>(uncompressed.data() + offset, _chunk_val.num_elements);
                        blosc2::decompress(
                            cpu_ctx.decompression_ctx,
                            subspan,
                            std::get<detail::cpu_chunk>(_chunk_val.value)
                        );
                        offset += _chunk_val.num_elements;
                    }
                }

                return uncompressed;
            }

            std::vector<T> chunk(blosc2::context_raw_ptr decompression_ctx, size_t index) const override
            {
                if (index > this->m_chunks.size() - 1)
                {
                    throw std::out_of_range(
                        std::format(
                            "Cannot access index {} in lazy-schunk. Total amount of chunks is {}",
                            index,
                            this->m_chunks.size()
                        )
                    );
                }

                const auto& chunk_val = std::get<cpu_container>(this->m_chunks.at(index));

                if (std::holds_alternative<cpu_chunk>(chunk_val.value))
                {
                    std::vector<T> uncompressed(this->chunk_elements(index), 0);
                    this->chunk(decompression_ctx, std::span<T>(uncompressed), index);
                    return uncompressed;
                }
                return std::vector<T>(this->chunk_elements(index), std::get<T>(chunk_val.value));
            }

            void chunk(blosc2::context_raw_ptr decompression_ctx, std::span<T> buffer, size_t index) const override
            {
                this->validate_chunk_index(index);
                if (this->is_gpu_chunk(index))
                {
                    throw std::runtime_error(
                        "Invalid function overload called for lazy_schunk::chunk. The given chunk is not a cpu"
                        " chunk but a gpu chunk."
                    );
                }

                // Either decompress from the compressed data or fill with the lazy chunks value
                if (const auto& chunk_val = std::get<cpu_container>(this->m_chunks.at(index)); std::holds_alternative<
                    cpu_chunk>(chunk_val.value))
                {
                    const auto& compressed = std::get<cpu_chunk>(chunk_val.value);
                    blosc2::decompress(
                        decompression_ctx,
                        buffer,
                        std::span<const std::byte>(compressed)
                    );
                }
                else
                {
                    std::fill(
                        std::execution::par_unseq,
                        buffer.begin(),
                        buffer.end(),
                        std::get<T>(chunk_val.value)
                    );
                }
            }

            void chunk(std::span<T> buffer, size_t index) const override
            {
                this->validate_chunk_index(index);
                if (!this->is_gpu_chunk(index))
                {
                    throw std::runtime_error(
                        "Invalid function overload called for lazy_schunk::chunk. The given chunk is not a gpu"
                        " chunk but a cpu chunk."
                    );
                }

                // Either decompress from the compressed data or fill with the lazy chunks value
                if (const auto& chunk_val = std::get<gpu_container>(this->m_chunks.at(index)); std::holds_alternative<
                    gpu_chunk<T>>(chunk_val.value))
                {
                    const auto& chunk_container = std::get<gpu_chunk<T>>(chunk_val.value);
                    auto compressor = cuda::make_compressor<T>(chunk_container);
                    std::visit(
                        [&](auto& _compressor)
                        {
                            _compressor.decompress(chunk_container, std::span<T>(buffer));
                        },
                        compressor
                    );
                }
                else
                {
                    std::fill(
                        std::execution::par_unseq,
                        buffer.begin(),
                        buffer.end(),
                        std::get<T>(chunk_val.value)
                    );
                }
            }

            void set_chunk(blosc2::context_ptr& compression_ctx,
                           std::span<T> uncompressed,
                           size_t index,
                           bool validate_chunk_sizes = true) override
            {
                this->validate_chunk_index(index);

                auto compressed = blosc2::compress_to_chunk<T>(compression_ctx, uncompressed);

                auto chunk = detail::lazy_chunk<T, cpu_chunk>{
                    std::move(compressed),
                    uncompressed.size()
                };
                this->m_chunks[index] = std::move(chunk);
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
                auto chunk = detail::lazy_chunk<T, gpu_chunk<T>>{
                    std::move(_chunk),
                    uncompressed.size()
                };
                this->m_chunks[index] = std::move(chunk);
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
                if (std::holds_alternative<cpu_compression_context>(compression_ctx))
                {
                    auto compressed = blosc2::compress_to_chunk<T>(
                        std::get<cpu_compression_context>(compression_ctx).compression_ctx,
                        uncompressed
                    );
                    auto chunk = detail::lazy_chunk<T, cpu_chunk>{
                        cpu_chunk(compressed.begin(), compressed.end()),
                        uncompressed.size()
                    };
                    this->m_chunks[index] = std::move(chunk);
                }
                else
                {
                    auto compressor = cuda::make_compressor<T>(
                        std::get<gpu_compression_context>(compression_ctx).ctx.codec
                    );
                    cuda::compressed_chunk<T> gpu_chunk{};
                    std::visit(
                        [&](auto& _compressor)
                        {
                            gpu_chunk = _compressor.compress(
                                uncompressed,
                                std::get<gpu_compression_context>(compression_ctx).ctx
                            );
                        },
                        compressor
                    );

                    auto chunk = detail::lazy_chunk<T, detail::gpu_chunk<T>>{
                        std::move(gpu_chunk),
                        uncompressed.size()
                    };
                    this->m_chunks[index] = std::move(chunk);
                }

                if (validate_chunk_sizes)
                {
                    this->validate_chunk_sizes();
                }
            };

            void append_chunk(cuda::nvcomp_context compression_ctx,
                              std::span<const T> uncompressed,
                              bool validate_chunk_sizes = true) override
            {
                auto compressor = cuda::make_compressor<T>(compression_ctx.codec);
                cuda::compressed_chunk<T> _chunk{};
                std::visit(
                    [&](auto& _compressor) -> void
                    {
                        _chunk = _compressor.compress(uncompressed, compression_ctx);
                    },
                    compressor
                );

                auto chunk = detail::lazy_chunk<T, gpu_chunk<T>>{
                    std::move(_chunk),
                    uncompressed.size()
                };
                this->m_chunks.push_back(std::move(chunk));
                if (validate_chunk_sizes)
                {
                    this->validate_chunk_sizes();
                }
            }

            void append_chunk(blosc2::context_ptr& compression_ctx,
                              std::span<T> uncompressed,
                              std::span<std::byte> compression_buff,
                              bool validate_chunk_sizes = true) override
            {
                auto csize = blosc2::compress<T>(compression_ctx, uncompressed, compression_buff);
                auto chunk = detail::lazy_chunk<T, cpu_chunk>{
                    cpu_chunk(compression_buff.begin(), compression_buff.begin() + csize),
                    uncompressed.size()
                };
                this->m_chunks.push_back(std::move(chunk));
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
                    auto chunk = detail::lazy_chunk<T, cpu_chunk>{
                        cpu_chunk(compressed.begin(), compressed.end()),
                        uncompressed.size()
                    };
                    this->m_chunks.push_back(std::move(chunk));
                }
                else
                {
                    auto compressor = cuda::make_compressor<T>(
                        std::get<gpu_compression_context>(compression_ctx).ctx.codec
                    );
                    cuda::compressed_chunk<T> gpu_chunk{};
                    std::visit(
                        [&](auto& _compressor)
                        {
                            gpu_chunk = _compressor.compress(
                                uncompressed,
                                std::get<gpu_compression_context>(compression_ctx).ctx
                            );
                        },
                        compressor
                    );

                    auto chunk = detail::lazy_chunk<T, detail::gpu_chunk<T>>{
                        std::move(gpu_chunk),
                        uncompressed.size()
                    };
                    this->m_chunks.push_back(std::move(chunk));
                }

                if (validate_chunk_sizes)
                {
                    this->validate_chunk_sizes();
                }
            };

            /// Retrieve the total compressed size of the lazy-schunk.
            /// Lazy chunks will count as the size of T.
            size_t csize() const noexcept override
            {
                size_t _csize = 0;
                size_t idx = 0;
                for (const auto& chunk : this->m_chunks)
                {
                    if (this->is_gpu_chunk(idx))
                    {
                        const auto& _chunk = std::get<gpu_container>(chunk);
                        if (std::holds_alternative<T>(_chunk.value))
                        {
                            _csize += sizeof(T);
                        }
                        else
                        {
                            _csize += std::get<gpu_chunk<T>>(_chunk.value).size();
                        }
                    }
                    else
                    {
                        const auto& _chunk = std::get<cpu_container>(chunk);
                        if (std::holds_alternative<T>(_chunk.value))
                        {
                            _csize += sizeof(T);
                        }
                        else
                        {
                            _csize += std::get<cpu_chunk>(_chunk.value).size();
                        }
                    }
                    ++idx;
                }
                return _csize;
            }

            // The total uncompressed size of the lazy-schunk in elements.
            size_t size() const noexcept override
            {
                size_t _size = 0;
                for (const auto& chunk : this->m_chunks)
                {
                    std::visit(
                        [&](const auto& _chunk) -> void
                        {
                            _size += _chunk.num_elements;
                        },
                        chunk
                    );
                }
                return _size;
            }

        private:
            /// Check whether this->m_chunks contain any still-lazy chunks.
            bool has_lazy_chunk() const noexcept
            {
                for (const auto& chunk : this->m_chunks)
                {
                    if (std::holds_alternative<T>(chunk.value))
                    {
                        return true;
                    }
                }
                return false;
            }

            /// Get the value of the first encountered lazy chunk, since we only create lazy chunks with a single value
            /// this is a valid way of accessing this value. if no lazy chunk exists we simply return T{}
            T lazy_chunk_value() const noexcept
            {
                for (const auto& chunk : this->m_chunks)
                {
                    T value = {};

                    std::visit(
                        [&](const auto& _chunk)
                        {
                            if (_chunk.is_lazy())
                            {
                                value = std::get<T>(_chunk.value);
                            }
                        },
                        chunk
                    );

                    if (value != T{})
                    {
                        return value;
                    }
                }

                return {};
            }
        };
    } // detail
} // NAMESPACE_COMPRESSED_IMAGE
