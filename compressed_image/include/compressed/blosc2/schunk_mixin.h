#pragma once

#include <span>
#include <vector>
#include <cstddef>

#include "compressed/macros.h"
#include "wrapper.h"
#include "compressed/constants.h"

namespace NAMESPACE_COMPRESSED_IMAGE
{
	namespace blosc2
	{

		namespace detail
		{

			/// Opaque mixin around a blosc2 super-chunk with the intention of not using a `blosc2_schunk`
			/// itself but instead of using it directly the chunks should be stored individually.
			/// Subclassed by either a `schunk` or a `lazy_schunk` depending on the needs of the 
			/// consumer.
			template <typename T, typename ContainerType>
			struct schunk_mixin
			{
				virtual ~schunk_mixin() = default;

				/// convert the struct into a blosc2 schunk.
				virtual blosc2::schunk_ptr to_schunk() = 0;

				/// Generate an uncompressed vector from all of the chunks.
				///
				/// \param decompression_ctx the decompression context pr.
				/// 
				/// \returns a contiguous vector representing the uncompressed schunk.
				virtual std::vector<T> to_uncompressed(blosc2::context_ptr& decompression_ctx) const = 0;

				/// Retrieve the uncompressed chunk at `index`.
				///
				/// \param decompression_ctx the decompression context ptr
				/// \param index the index of the chunk within the schunk.
				/// 
				/// \throws std::out_of_range if the index is not valid
				virtual std::vector<T> chunk(blosc2::context_ptr& decompression_ctx, size_t index) const = 0;

				/// Retrieve the uncompressed chunk at `index`.
				///
				/// \param decompression_ctx the decompression context ptr
				/// \param index the index of the chunk within the schunk.
				/// 
				/// \throws std::out_of_range if the index is not valid
				virtual std::vector<T> chunk(blosc2::context_raw_ptr decompression_cx, size_t index) const = 0;

				/// Retrieve the uncompressed chunk at `index`.
				///
				/// \param decompression_ctx the decompression context ptr
				/// \param buffer the buffer to fill the uncompressed data with. Must be at least max chunk size.
				/// \param index the index of the chunk within the schunk.
				/// 
				/// \throws std::out_of_range if the index is not valid
				virtual void chunk(blosc2::context_ptr& decompression_ctx, std::span<T> buffer, size_t index) const = 0;

				/// Retrieve the uncompressed chunk at `index`.
				///
				/// \param decompression_ctx the decompression context ptr
				/// \param buffer the buffer to fill the uncompressed data with. Must be at least max chunk size.
				/// \param index the index of the chunk within the schunk.
				/// 
				/// \throws std::out_of_range if the index is not valid
				virtual void chunk(blosc2::context_raw_ptr decompression_ctx, std::span<T> buffer, size_t index) const = 0;

				/// Set the chunk at `index` to the compressed data.
				///
				/// \param compressed the compressed chunk
				/// \param index the index of the chunk within the schunk.
				/// 
				/// \throws std::out_of_range if the index is not valid
				virtual void set_chunk(std::vector<std::byte> compressed, size_t index) = 0;

				/// Set the chunk at `index` to the compressed data.
				///
				/// \param compressed the compressed chunk
				/// \param index the index of the chunk within the schunk.
				/// 
				/// \throws std::out_of_range if the index is not valid
				virtual void set_chunk(std::span<const std::byte> compressed, size_t index) = 0;

				/// Set the chunk at `index` to the uncompressed data (compressing it).
				///
				/// \param compression_ctx the compression context to use for compression.
				/// \param uncompressed the uncompressed chunk
				/// \param index the index of the chunk within the schunk.
				/// 
				/// \throws std::out_of_range if the index is not valid
				virtual void set_chunk(blosc2::context_ptr& compression_ctx, std::span<T> uncompressed, size_t index) = 0;

				/// Append to the schunk with the uncompressed data (compressing it).
				///
				/// \param compressed the compressed chunk
				virtual void append_chunk(std::vector<std::byte> compressed) = 0;

				/// Append to the schunk with the uncompressed data (compressing it).
				///
				/// \param compression_ctx the compression context to use for compression.
				/// \param uncompressed the uncompressed chunk
				virtual void append_chunk(blosc2::context_ptr& compression_ctx, std::span<T> uncompressed) = 0;

				/// Append to the schunk with the uncompressed data (compressing it).
				///
				/// \param compression_ctx the compression context to use for compression.
				/// \param uncompressed the uncompressed chunk
				/// \param compression_buff the compression buffer to use for temporary storage.
				virtual void append_chunk(blosc2::context_ptr& compression_ctx, std::span<T> uncompressed, std::span<std::byte> compression_buff) = 0;

				/// Retrieve the number of elements (uncompressed) that the schunk stores.
				///
				/// \throws std::runtime_error if the chunk_bytes / sizeof(T) is not cleanly divisble
				size_t chunk_elements() const
				{
					auto _size =  this->chunk_bytes();
					if (_size % sizeof(T) != 0)
					{
						throw std::runtime_error(
							std::format(
								"Internal Error: The chunk byte size is not cleanly divisible by the sizeof T." 
								" Chunk size is {:L} while sizeof(T) is {}", _size, sizeof(T)
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
								" Chunk size is {:L} while sizeof(T) is {}", _size, sizeof(T)
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
					return this->m_ChunkSize;
				};
				
				/// Retrieve the number of bytes stored by the chunk at index `index`. This will be equivalent to 
				/// chunk_bytes unless it is the last chunk in which case it may be smaller.
				/// 
				/// \throws std::out_of_range if the index is not valid in the super-chunk.
				virtual size_t chunk_bytes(size_t index) const = 0;

				/// The number of chunks in the super-chunk
				size_t num_chunks() const noexcept
				{
					return m_Chunks.size();
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

			protected:
				std::vector<ContainerType> m_Chunks{};
				/// The maximum size a chunk is constrained to, in bytes. This will dictate the size of all chunks from
				///  0 - (this->m_Chunks.size() - 1). The last chunk may be any other size smaller than or equal to this value.
				size_t m_ChunkSize = s_default_chunksize;

				/// Validate the chunk index throwing a std::out_of_range if the index is not valid.
				void validate_chunk_index(size_t index) const
				{
					if (index > m_Chunks.size() - 1)
					{
						throw std::out_of_range(
							std::format("Cannot access index {} in schunk. Total amount of chunks is {}", index, m_Chunks.size())
						);
					}
				}

				/// Validate all the chunk sizes currently held by the super-chunk. This function
				/// ensures that the chunks 
				void validate_chunk_sizes() const
				{
					// Check that all chunks barring the last one are equal to m_ChunkSize
					for (auto i : std::views::iota(size_t{ 0 }, this->num_chunks() - 1))
					{
						if (this->chunk_bytes(i) != this->chunk_bytes())
						{
							throw std::runtime_error(
								std::format(
									"Error while validating chunk sizes; Expected all chunks to have a size equivalent to {:L} (m_ChunkSize)."
									" However, chunk {} instead has a chunk size of {:L}. Having a size different from the rest of the chunks"
									" is only supported for the last chunk (blosc2 limitation). Please ensure that all chunks are equally sized"
									" when modifying the super-chunk (excluding the last one).",
									this->chunk_bytes(), i, this->chunk_bytes(i)
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
								this->chunk_bytes(), this->chunk_bytes(this->num_chunks() - 1)
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

	} // blosc2

} // NAMESPACE_COMPRESSED_IMAGE