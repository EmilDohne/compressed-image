#pragma once

#include <span>
#include <vector>
#include <cstddef>

#include "compressed/macros.h"
#include "wrapper.h"
#include "compressed/constants.h"
#include "compressed/context.h"
#include "compressed/cuda/compressors/base.h"


namespace
NAMESPACE_COMPRESSED_IMAGE
{
    namespace detail
    {
        /// \brief The default storage class for a gpu compressed chunk.
        ///
        /// \note this chunk may not live on the gpu, this just indicates it was generated
        ///		  on the gpu.
        template <typename T>
        using gpu_chunk = cuda::compressed_chunk<T>;

        /// \brief The default storage class for a cpu compressed chunk.
        using cpu_chunk = util::default_init_vector<std::byte>;

        /// Mixin for representing a blosc2-style super-chunk for both cpu and gpu chunks.
        ///
        /// \tparam _gpu_container_type The type for a gpu compressed chunk
        /// \tparam _cpu_container_type The type for a cpu compressed chunk
        template <typename T, typename _gpu_container_type = gpu_chunk<T>, typename _cpu_container_type = cpu_chunk>
        struct schunk_mixin
        {
            using gpu_container = _gpu_container_type;
            using cpu_container = _cpu_container_type;

            virtual ~schunk_mixin() = default;

            /// Checks whether the chunk at `index` is a gpu/cpu chunk
            ///
            /// \parm index The chunk index
            ///
            /// \throws std::runtime_error if the chunk index is not valid
            bool is_gpu_chunk(size_t index) const
            {
                if (index > m_chunks.size() - 1)
                {
                    throw std::runtime_error(
                        std::format(
                            "Invalid chunk index {}, can at most index up to {}",
                            index,
                            m_chunks.size() - 1
                        )
                    );
                }

                return std::holds_alternative<_gpu_container_type>(m_chunks.at(index));
            };

            /// Generate an uncompressed vector from all of the chunks.
            ///
            /// \param cpu_ctx the decompression context for all cpu based chunks.
            /// \param gpu_ctx the decompression context for all gpu based chunks.
            ///
            /// \returns a contiguous vector representing the uncompressed schunk.
            virtual std::vector<T> to_uncompressed(
                cpu_compression_context& cpu_ctx,
                [[maybe_unused]] gpu_compression_context gpu_ctx
            ) const
            {
                _COMPRESSED_PROFILE_FUNCTION();
                auto num_elems = this->size();
                std::vector<T> data(num_elems);

                size_t data_offset = 0;
                for (auto idx : std::views::iota(size_t{0}, this->m_chunks.size()))
                {
                    size_t chunk_elems = this->chunk_elements(idx);

                    auto subspan = std::span<T>(data.data() + data_offset, chunk_elems);

                    if (this->is_gpu_chunk(idx))
                    {
                        this->chunk(subspan, idx);
                    }
                    else
                    {
                        if (!cpu_ctx.decompression_ctx || !cpu_ctx.compression_ctx)
                        {
                            throw std::invalid_argument(
                                std::format(
                                    "Chunk {}: valid cpu decompression and compression contexts must be provided"
                                    " for cpu chunks",
                                    idx
                                )
                            );
                        }

                        this->chunk(cpu_ctx.decompression_ctx.get(), subspan, idx);
                    }

                    data_offset += chunk_elems;
                }

                return data;
            };

            /// Generate an uncompressed vector from all of the chunks.
            ///
            /// This overload may only be called if the schunk contains no gpu chunks.
            ///
            /// \param context the decompression context for the chunks
            ///
            /// \throws std::runtime_error if the schunk contains one or more gpu chunks.
            ///
            /// \returns a contiguous vector representing the uncompressed schunk.
            std::vector<T> to_uncompressed(cpu_compression_context& context) const
            {
                for (size_t i = 0; i < this->num_chunks(); ++i)
                {
                    if (is_gpu_chunk(i))
                    {
                        throw std::runtime_error(
                            std::format(
                                "Invalid overload of 'to_uncompressed' called. This overload may only be called if"
                                " there are no GPU chunks. However, at least chunk {} is a gpu chunk. Please pass"
                                " an explicit GPU decompressor.",
                                i
                            )
                        );
                    }
                }
                return this->to_uncompressed(context, gpu_compression_context{cuda::nvcomp_context{}});
            }

            /// Generate an uncompressed vector from all of the chunks.
            ///
            /// This overload may only be called if the schunk contains no cpu chunks.
            ///
            /// \param context the decompression context for the chunks
            ///
            /// \throws std::runtime_error if the schunk contains one or more gpu chunks.
            ///
            /// \returns a contiguous vector representing the uncompressed schunk.
            std::vector<T> to_uncompressed(gpu_compression_context context) const
            {
                for (size_t i = 0; i < this->num_chunks(); ++i)
                {
                    if (!is_gpu_chunk(i))
                    {
                        throw std::runtime_error(
                            std::format(
                                "Invalid overload of 'to_uncompressed' called. This overload may only be called if"
                                " there are no CPU chunks. However, at least chunk {} is a cpu chunk. Please pass"
                                " an explicit CPU decompressor.",
                                i
                            )
                        );
                    }
                }
                auto _cpu_context = cpu_compression_context{};
                return this->to_uncompressed(_cpu_context, context);
            }

            /// Retrieve the uncompressed chunk at `index`.
            ///
            /// \param context the decompression context
            /// \param index the index of the chunk within the schunk.
            ///
            /// \throws std::out_of_range if the index is not valid
            virtual std::vector<T> chunk(cpu_compression_context& context, const size_t index) const
            {
                return this->chunk(context.decompression_ctx.get(), index);
            };

            /// Retrieve the uncompressed gpu chunk at `index`.
            ///
            /// \param context the decompression context
            /// \param index the index of the chunk within the schunk.
            ///
            /// \throws std::out_of_range if the index is not valid
            virtual std::vector<T> chunk(const cuda::nvcomp_context context, const size_t index) const
            {
                std::vector<T> buffer(this->size());
                this->chunk(context, index);
                return buffer;
            };

            /// Retrieve the uncompressed chunk at `index`.
            ///
            /// \param decompression_ctx the decompression context ptr
            /// \param index the index of the chunk within the schunk.
            ///
            /// \throws std::out_of_range if the index is not valid
            virtual std::vector<T> chunk(blosc2::context_raw_ptr decompression_ctx, size_t index) const
            {
                std::vector<T> buffer(this->chunk_elements(index));
                this->chunk(decompression_ctx, std::span<T>(buffer), index);
                return buffer;
            };

            /// Retrieve the uncompressed chunk at `index`.
            ///
            /// \param decompression_ctx the decompression context ptr
            /// \param buffer the buffer to fill the uncompressed data with. Must be at least max chunk size.
            /// \param index the index of the chunk within the schunk.
            ///
            /// \throws std::out_of_range if the index is not valid
            virtual void chunk(blosc2::context_ptr& decompression_ctx, std::span<T> buffer, size_t index) const
            {
                this->chunk(decompression_ctx.get(), buffer, index);
            };

            /// Retrieve the uncompressed gpu chunk at `index`.
            ///
            /// \param buffer the buffer to fill the uncompressed data with. Must be at least max chunk size.
            /// \param index the index of the chunk within the schunk.
            ///
            /// \throws std::out_of_range if the index is not valid
            virtual void chunk(std::span<T> buffer, size_t index) const = 0;

            /// Retrieve the uncompressed chunk at `index`.
            ///
            /// \param decompression_ctx the decompression context ptr
            /// \param buffer the buffer to fill the uncompressed data with. Must be at least max chunk size.
            /// \param index the index of the chunk within the schunk.
            ///
            /// \throws std::out_of_range if the index is not valid
            virtual void chunk(blosc2::context_raw_ptr decompression_ctx, std::span<T> buffer, size_t index) const = 0;

            /// Set the chunk at `index` to the uncompressed data (compressing it).
            ///
            /// \param compression_ctx the compression context to use for compression.
            /// \param uncompressed the uncompressed chunk
            /// \param index the index of the chunk within the schunk.
            ///
            /// \throws std::out_of_range if the index is not valid
            virtual void set_chunk(blosc2::context_ptr& compression_ctx, std::span<T> uncompressed, size_t index) = 0;

            /// Set the chunk at `index` to the uncompressed data (compressing it).
            ///
            /// \param compression_ctx the compression context to use for compression.
            /// \param uncompressed the uncompressed chunk
            /// \param index the index of the chunk within the schunk.
            ///
            /// \throws std::out_of_range if the index is not valid
            virtual void set_chunk(cuda::nvcomp_context compression_ctx, std::span<T> uncompressed, size_t index) = 0;


            /// Append to the schunk with the uncompressed data (compressing it).
            ///
            /// \param compression_ctx the compression context to use for compression.
            /// \param uncompressed the uncompressed chunk
            virtual void append_chunk(cuda::nvcomp_context compression_ctx, std::span<const T> uncompressed) = 0;

            /// Append to the schunk with the uncompressed data (compressing it).
            ///
            /// \param compression_ctx the compression context to use for compression.
            /// \param uncompressed the uncompressed chunk
            /// \param compression_buff the compression buffer to use for temporary storage.
            virtual void append_chunk(blosc2::context_ptr& compression_ctx,
                                      std::span<T> uncompressed,
                                      std::span<std::byte> compression_buff) = 0;

            /// Retrieve the number of elements (uncompressed) that the schunk stores.
            ///
            /// \throws std::runtime_error if the chunk_bytes / sizeof(T) is not cleanly divisble
            size_t chunk_elements() const
            {
                auto _size = this->chunk_bytes();
                if (_size % sizeof(T) != 0)
                {
                    throw std::runtime_error(
                        std::format(
                            "Internal Error: The chunk byte size is not cleanly divisible by the sizeof T."
                            " Chunk size is {:L} while sizeof(T) is {}",
                            _size,
                            sizeof(T)
                        )
                    );
                }
                return _size / sizeof(T);
            };

            /// Retrieve the number of elements (uncompressed) that the schunk stores at a given chunk.
            /// In all cases except for chunk_elements(num_chunks() - 1) this will return chunk_elements.
            ///
            /// \throws std::out_of_range if the index is not valid in the super-chunk.
            /// \throws std::runtime_error if the chunk_bytes / sizeof(T) is not cleanly divisble
            size_t chunk_elements(size_t index) const
            {
                auto _size = this->chunk_bytes(index);
                if (_size % sizeof(T) != 0)
                {
                    throw std::runtime_error(
                        std::format(
                            "Internal Error: The chunk byte size is not cleanly divisible by the sizeof T."
                            " Chunk size is {:L} while sizeof(T) is {}",
                            _size,
                            sizeof(T)
                        )
                    );
                }
                return _size / sizeof(T);
            };

            /// Retrieve the number of bytes stored by the super-chunk per-chunk. This will be equivalent
            /// to the number of uncompressed bytes stored by each chunk up to num_chunks() - 1.
            /// The last chunk may be smaller (but not bigger) in size than this value.
            size_t chunk_bytes() const
            {
                return this->m_chunk_size;
            };

            /// Retrieve the number of bytes stored by the chunk at index `index`. This will be equivalent to
            /// chunk_bytes unless it is the last chunk in which case it may be smaller.
            ///
            /// \throws std::out_of_range if the index is not valid in the super-chunk.
            virtual size_t chunk_bytes(size_t index) const = 0;

            /// The number of chunks in the super-chunk
            size_t num_chunks() const noexcept
            {
                return m_chunks.size();
            }

            /// The total compressed size of the schunk in bytes
            virtual size_t csize() const noexcept = 0;

            /// The total uncompressed size of the schunk in elements
            virtual size_t size() const noexcept = 0;

            /// The total number of bytes stored in the schunk when uncompressed.
            /// equivalent to size() * sizeof(T)
            size_t byte_size() const noexcept
            {
                return size() * sizeof(T);
            }

            size_t max_chunk_size() const noexcept
            {
                return m_chunk_size;
            }

            size_t max_block_size() const noexcept
            {
                return m_block_size;
            }

        protected:
            std::vector<std::variant<_cpu_container_type, _gpu_container_type>> m_chunks{};
            /// The maximum size a chunk is constrained to, in bytes. This will dictate the size of all chunks from
            ///  0 - (this->m_chunks.size() - 1). The last chunk may be any other size smaller than or equal to this value.
            size_t m_chunk_size = s_default_chunksize;
            size_t m_block_size = s_default_blocksize;

            /// Validate the chunk index throwing a std::out_of_range if the index is not valid.
            void validate_chunk_index(size_t index) const
            {
                if (index > m_chunks.size() - 1)
                {
                    throw std::out_of_range(
                        std::format(
                            "Cannot access index {} in schunk. Total amount of chunks is {}",
                            index,
                            m_chunks.size()
                        )
                    );
                }
            }

            /// Validate all the chunk sizes currently held by the super-chunk. This function
            /// ensures that the chunks
            void validate_chunk_sizes() const
            {
                // Check that all chunks barring the last one are equal to m_chunk_size
                for (auto i : std::views::iota(size_t{0}, this->num_chunks() - 1))
                {
                    if (this->chunk_bytes(i) != this->chunk_bytes())
                    {
                        throw std::invalid_argument(
                            std::format(
                                "Error while validating chunk sizes; Expected all chunks to have a size equivalent to {:L} (m_chunk_size)."
                                " However, chunk {} instead has a chunk size of {:L}. Having a size different from the rest of the chunks"
                                " is only supported for the last chunk (blosc2 limitation). Please ensure that all chunks are equally sized"
                                " when modifying the super-chunk (excluding the last one).",
                                this->chunk_bytes(),
                                i,
                                this->chunk_bytes(i)
                            )
                        );
                    }
                }

                // Check that the last chunk is not larger than the rest.
                if (this->chunk_bytes(this->num_chunks() - 1) > this->chunk_bytes())
                {
                    throw std::runtime_error(
                        std::format(
                            "Error while validating chunk sizes; Expected the last chunk to be at most {:L} bytes,"
                            " instead got {:L} bytes.",
                            this->chunk_bytes(),
                            this->chunk_bytes(this->num_chunks() - 1)
                        )
                    );
                }
            }

            /// Get the buffer size for T for the given byte size. Checks that the buffer
            /// can be divided cleanly by sizeof(T).
            size_t get_T_buffer_size(size_t byte_size) const
            {
                if (byte_size % sizeof(T) != 0)
                {
                    throw std::runtime_error(
                        std::format(
                            "Cannot get buffer size for type T of size {} because it is not evenly divisible for buffer size {:L}",
                            sizeof(T),
                            byte_size
                        )
                    );
                }
                return byte_size / sizeof(T);
            }
        };
    } // detail
} // NAMESPACE_COMPRESSED_IMAGE
