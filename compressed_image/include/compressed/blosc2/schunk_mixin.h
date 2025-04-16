#pragma once

#include <span>
#include <vector>
#include <cstddef>

#include "compressed/macros.h"
#include "wrapper.h"

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

				/// Retrieve the number of elements (uncompressed) that the chunk at index `index` stores.
				///
				/// \param index The chunk index, must be valid.
				/// \throws std::out_of_range if the `index` is invalid
				virtual size_t chunk_size(size_t index) const = 0;

				/// The number of chunks in the super-chunk
				size_t num_chunks() const noexcept
				{
					return m_Chunks.size();
				}

				/// The total compressed size of the schunk
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
			};

		} // detail

	} // blosc2

} // NAMESPACE_COMPRESSED_IMAGE