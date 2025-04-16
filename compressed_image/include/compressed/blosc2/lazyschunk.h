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

namespace NAMESPACE_COMPRESSED_IMAGE
{

	namespace blosc2
	{

		namespace detail
		{
			/// Wrapper representing a lazy chunk holding either an initialized (and compressed) chunk
			/// in the form of a byte array or just a single T representing a lazy state
			template <typename T>
			struct lazy_chunk
			{
				std::variant<std::vector<std::byte>, T> value;
				size_t num_elements = 0;

				size_t byte_size() const noexcept
				{
					return num_elements * sizeof(T);
				}

				bool is_lazy() const noexcept
				{
					return std::holds_alternative<T>(this->value);
				}
			};
		}


		template <typename T>
		struct lazy_schunk final : public detail::schunk_mixin<T, detail::lazy_chunk<T>>
		{

			/// Initialize a lazy super-chunk from the given value, has a near-zero
			/// cost with the chunks only being initialized on read/modify.
			/// 
			/// \param value The initial value to fill.
			/// \param num_elements The size to initialize the data with.
			/// \param chunk_size The requested chunk size. It is up to the caller to ensure
			///                   this is appropriately sized (i.e. by using util::align_chunk_to_scanlines)
			lazy_schunk(T value, size_t num_elements, const size_t chunk_size)
			{
				util::validate_chunk_size<T>(chunk_size, "lazy_schunk");
				m_ChunkSize = chunk_size;

				size_t num_bytes = num_elements * sizeof(T);

				// Calculate all 'full' chunks and the final remainder (if any).
				size_t num_full_chunks = num_bytes / m_ChunkSize;
				size_t remainder_bytes = num_bytes - (m_ChunkSize * num_full_chunks);

				// Initialize lazy chunks with the provided value of T
				for (auto idx : std::views::iota(num_full_chunks))
				{
					detail::lazy_chunk<T> chunk = { value, m_ChunkSize / sizeof(T) };
					this->m_Chunks.push_back(std::move(chunk));
				}
				if (remainder_bytes > 0)
				{
					detail::lazy_chunk<T> chunk = { value, remainder_bytes / sizeof(T) };
					this->m_Chunks.push_back(std::move(chunk));
				}
			}

			/// convert the lazy schunk into a super-chunk, generating any 
			/// not yet initialized lazy chunks in the process. This should 
			/// be done once all the data is computed to minimize the overhead. 
			schunk_ptr to_schunk() override
			{
				_COMPRESSED_PROFILE_FUNCTION();
				// Initialize the chunks, either appending the byte array directly to the schunk 
				// or compressing the lazy chunk.
				blosc2::schunk_ptr schunk = create_default_schunk();

				// Allocate and compress the lazy buff. Since this only needs to happen once
				// as all lazy values are the same we can just use the same compressed buffer for all.
				util::default_init_vector<std::byte> lazy_compressed_data;
				if (this->has_lazy_chunk())
				{
					auto lazy_buff = std::vector<T>(this->get_T_buffer_size(this->chunk_size, this->lazy_chunk_value()));
					lazy_compressed_data.resize(blosc2::min_compressed_size<chunk_size>());

					auto context = blosc2::create_compression_context(schunk, std::thread::hardware_concurrency(), enums::codec::lz4, 9);
					blosc2::compress(context, std::span<T>(lazy_buff), std::span<std::byte>(lazy_compressed_data));
				}

				// Iterate all the chunks, if lazy add the compressed lazy buffer, else add the compressed data.
				for (auto& chunk : this->m_Chunks)
				{
					if (std::holds_alternative<std::vector<std::byte>>(chunk.value))
					{
						auto& data = std::get<std::vector<std::byte>>(chunk.value);
						blosc2_schunk_append_chunk(
							schunk.get(),
							reinterpret_cast<uint8_t*>(data.data()),
							true // copy
						);
					}
					else
					{
						assert(lazy_compressed_data.size() >= BLOSC2_MAX_OVERHEAD);
						// we already initialized the buffer to the lazychunk value above
						blosc2_schunk_append_chunk(
							schunk.get(),
							reinterpret_cast<uint8_t*>(lazy_compressed_data.data()),
							true // copy
						);
					}
				}

				return std::move(schunk);
			}

			/// Generate an uncompressed vector from the chunks, using the decompression context
			/// to perform the decompression.
			std::vector<T> to_uncompressed(blosc2::context_ptr& decompression_ctx) const override
			{
				std::vector<T> uncompressed(this->size(), this->lazy_chunk_value());

				size_t offset = 0; // element offset
				for (const auto& chunk : this->m_Chunks)
				{
					if (std::holds_alternative<std::vector<std::byte>>(chunk.data))
					{
						auto subspan = std::span<T>(uncompressed.data() + offset, chunk.uncompressed_size);
						blosc2::decompress(decompression_ctx, subspan, std::get<std::vector<std::byte>>(chunk.data));
					}
					// Since we already initialized the uncompressed data to the lazy chunks' value we don't need
					// to do any filling here.
					offset += chunk.uncompressed_size;
				}

				return uncompressed;
			}

			std::vector<T> chunk(blosc2::context_ptr& decompression_ctx, size_t index) const override
			{
				return this->chunk(decompression_ctx.get(), index);
			}

			std::vector<T> chunk(blosc2::context_raw_ptr decompression_ctx, size_t index) const override
			{
				if (index > this->m_Chunks.size() - 1)
				{
					throw std::out_of_range(
						std::format("Cannot access index {} in lazy-schunk. Total amount of chunks is {}", index, this->m_Chunks.size())
					);
				}

				if (std::holds_alternative<std::vector<std::byte>>(this->m_Chunks[index].value))
				{
					std::vector<T> uncompressed(this->chunk_size(index), 0);
					this->chunk(decompression_ctx, std::span<T>(uncompressed), index);
					return uncompressed;
				}
				return std::vector<T>(this->chunk_size(index), std::get<T>(this->m_Chunks[index].value));
			}

			void chunk(blosc2::context_ptr& decompression_ctx, std::span<T> buffer, size_t index) const override
			{
				this->chunk(decompression_ctx.get(), buffer, index);
			}

			void chunk(blosc2::context_raw_ptr decompression_ctx, std::span<T> buffer, size_t index) const override
			{
				if (index > this->m_Chunks.size() - 1)
				{
					throw std::out_of_range(
						std::format("Cannot access index {} in lazy-schunk. Total amount of chunks is {}", index, this->m_Chunks.size())
					);
				}

				// Either decompress from the compressed data or fill with the lazy chunks value
				if (std::holds_alternative<std::vector<std::byte>>(chunk.value))
				{
					auto& compressed = std::get<std::vector<std::byte>>(this->m_Chunks[index]);
					blosc2::decompress(
						decompression_ctx,
						buffer,
						std::span<std::byte>(compressed)
					);
				}
				else
				{
					std::fill(
						std::execution::par_unseq,
						buffer.begin(),
						buffer.end(),
						std::get<T>(this->m_Chunks[index].value)
					);
				}
			}

			/// Set the chunk at `index` to the compressed data.
			void set_chunk(std::vector<std::byte> compressed, size_t index) override
			{
				this->m_Chunks[index].value = std::move(compressed);
			}

			/// Retrieve the number of elements (uncompressed) that the chunk at index `index` stores.
			///
			/// \param index The chunk index, must be valid.
			/// \throws std::out_of_range if the `index` is invalid
			size_t chunk_size(size_t index) const override
			{
				if (index > this->m_Chunks.size() - 1)
				{
					throw std::out_of_range(
						std::format("Cannot access index {} in lazy-schunk. Total amount of chunks is {}", index, this->m_Chunks.size())
					);
				}

				return this->m_Chunks[index].uncompressed_size;
			}

			size_t chunk_size() const noexcept
			{
				return m_ChunkSize;
			}

			/// Retrieve the total compressed size of the lazy-schunk.
			/// Lazy chunks will count as the size of T.
			size_t csize() const noexcept override
			{
				size_t _csize = 0;
				for (const auto& chunk : this->m_Chunks)
				{
					if (std::holds_alternative<T>(chunk.value))
					{
						_csize += sizeof(T);
					}
					else
					{
						_csize += std::get<std::vector<std::byte>>(chunk.value).size();
					}
				}
				return _csize;
			}

			// The total uncompressed size of the lazy-schunk in elements.
			size_t size() const noexcept override
			{
				size_t _size = 0;
				for (const auto chunk : this->m_Chunks)
				{
					size += chunk.uncompressed_size;
				}
				return _size;
			}

		private:
			/// The maximum size a chunk is constrained to, in bytes. This will dictate the size of all chunks from
			///  0 - (this->m_Chunks.size() - 1). The last chunk may be any other size smaller than or equal to this value.
			size_t m_ChunkSize = 0;

			/// Check whether this->m_Chunks contain any still-lazy chunks.
			bool has_lazy_chunk() const noexcept
			{
				for (const auto& chunk : this->m_Chunks)
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
				for (const auto& chunk : this->m_Chunks)
				{
					if (std::holds_alternative<T>(chunk.value))
					{
						return std::get<T>(chunk.value);
					}
				}
				return T{};

			}

			/// Get the buffer size for T for the given byte size. Checks that the buffer
			/// can be divided cleanly by sizeof(T).
			size_t get_T_buffer_size(size_t byte_size)
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

	} // blosc2

} // NAMESPACE_COMPRESSED_IMAGE