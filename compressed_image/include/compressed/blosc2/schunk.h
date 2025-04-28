#pragma once

#include <span>
#include <vector>
#include <cstddef>
#include <cassert>

#include "compressed/macros.h"
#include "compressed/util.h"
#include "compressed/blosc2/util.h"
#include "wrapper.h"
#include "schunk_mixin.h"

namespace NAMESPACE_COMPRESSED_IMAGE
{

	namespace blosc2
	{

		template <typename T>
		struct schunk final: public detail::schunk_mixin<T, std::vector<std::byte>>
		{
			using detail::schunk_mixin<T, std::vector<std::byte>>::chunk_bytes;

			schunk() = default;

			/// Initialize an empty schunk with just a schunk size. The data can then later
			/// be filled with append_chunk for example.
			schunk(const size_t chunk_size)
			{
				util::validate_chunk_size<T>(chunk_size, "schunk");
				this->m_ChunkSize = chunk_size;
			}

			/// Initialize a super-chunk from the given vector, compressing it
			/// 
			/// \param data The data to store
			/// \param chunk_size The requested chunk size. It is up to the caller to ensure
			///                   this is appropriately sized (i.e. by using util::align_chunk_to_scanlines)
			/// \param compression_ctx The compression context to be used for compressing the data.
			schunk(std::span<const T> data, const size_t chunk_size, blosc2::context_ptr& compression_ctx)
			{
				util::validate_chunk_size<T>(chunk_size, "schunk");
				this->m_ChunkSize = chunk_size;

				// Compression buffer we will continuously overwrite in our compression, the chunk data is then copied out
				// of this on initialization.
				util::default_init_vector<std::byte> compression_buffer(blosc2::min_compressed_size(chunk_size));
				auto compression_span = std::span<std::byte>(compression_buffer);

				size_t num_elements = data.size();
				size_t num_bytes = num_elements * sizeof(T);

				// Calculate all 'full' chunks and the final remainder (if any).
				size_t num_full_chunks = num_bytes / this->chunk_bytes();
				size_t remainder_bytes = num_bytes - (this->chunk_bytes() * num_full_chunks);

				size_t data_offset = 0;
				// Initialize the chunks by compressing them.
				for ([[maybe_unused]] auto idx : std::views::iota(size_t{ 0 }, num_full_chunks))
				{
					auto subspan = std::span<const T>(data.data() + data_offset, this->chunk_elements());
					auto csize = blosc2::compress<T>(compression_ctx, subspan, compression_span);

					// copy over a new vector containing all the elements from the compression span.
					this->m_Chunks.push_back(std::vector<std::byte>(compression_span.begin(), compression_span.begin() + csize));

					data_offset += this->chunk_elements();
				}
				if (remainder_bytes > 0)
				{
					auto subspan = std::span<const T>(data.data() + data_offset, data.size() - data_offset);
					auto csize = blosc2::compress<T>(compression_ctx, subspan, compression_span);

					// copy over a new vector containing all the elements from the compression span.
					this->m_Chunks.push_back(std::vector<std::byte>(compression_span.begin(), compression_span.begin() + csize));

					// no need to move over the data_offset.
				}
			}

			schunk_ptr to_schunk() override
			{
				_COMPRESSED_PROFILE_FUNCTION();
				blosc2::schunk_ptr schunk = create_default_schunk();
				for (auto& chunk : this->m_Chunks)
				{
					blosc2_schunk_append_chunk(
						schunk.get(),
						reinterpret_cast<uint8_t*>(chunk.data()),
						true // copy, blosc2 will internally at some point do this anyways.
					);
				}

				return schunk;
			}

			std::vector<T> to_uncompressed(blosc2::context_ptr& decompression_ctx) const override
			{
				_COMPRESSED_PROFILE_FUNCTION();
				auto num_elems = this->size();
				std::vector<T> data(num_elems);

				size_t data_offset = 0;
				for (auto idx : std::views::iota(size_t{ 0 }, this->m_Chunks.size()))
				{
					size_t chunk_elems = this->chunk_elements(idx);

					auto subspan = std::span<T>(data.data() + data_offset, chunk_elems);
					this->chunk(decompression_ctx, subspan, idx);

					data_offset += chunk_elems;
				}

				return data;
			}

			std::vector<T> chunk(blosc2::context_ptr& decompression_ctx, size_t index) const override
			{
				return this->chunk(decompression_ctx.get(), index);
			}

			std::vector<T> chunk(blosc2::context_raw_ptr decompression_ctx, size_t index) const override
			{
				this->validate_chunk_index(index);

				std::vector<T> decompressed(this->chunk_elements(index));
				auto chunk_span = std::span<const std::byte>(this->m_Chunks[index].begin(), this->m_Chunks[index].end());
				blosc2::decompress(decompression_ctx, std::span<T>(decompressed), chunk_span);

				return std::move(decompressed);
			}

			void chunk(blosc2::context_ptr& decompression_ctx, std::span<T> buffer, size_t index) const override
			{
				this->chunk(decompression_ctx.get(), buffer, index);
			}

			void chunk(blosc2::context_raw_ptr decompression_ctx, std::span<T> buffer, size_t index) const override
			{
				this->validate_chunk_index(index);

				if (buffer.size() < this->chunk_elements(index))
				{
					throw std::invalid_argument(
						std::format(
							"Unable to decompress chunk at idx {} into buffer as the buffer needs to at least have the size {:L}."
							" Instead got {:L}", index, this->chunk_elements(index), buffer.size()
						)
					);
				}

				auto chunk_span = std::span<const std::byte>(this->m_Chunks[index].begin(), this->m_Chunks[index].end());
				blosc2::decompress(decompression_ctx, std::span<T>(buffer), chunk_span);
			}

			void set_chunk(std::vector<std::byte> compressed, size_t index) override
			{
				this->validate_chunk_index(index);
				this->m_Chunks[index] = std::move(compressed);
				this->validate_chunk_sizes();
			}

			void set_chunk(std::span<const std::byte> compressed, size_t index) override
			{
				this->validate_chunk_index(index);
				this->m_Chunks[index] = std::vector<std::byte>(compressed.begin(), compressed.end());
				this->validate_chunk_sizes();
			}

			void set_chunk(blosc2::context_ptr& compression_ctx, std::span<T> uncompressed, size_t index) override
			{
				this->validate_chunk_index(index);

				util::default_init_vector<std::byte> compression_buffer(blosc2::min_compressed_size(this->chunk_bytes()));
				std::span<std::byte> compression_span(compression_buffer);

				auto csize = blosc2::compress<T>(compression_ctx, uncompressed, compression_span);

				// copy over a new vector containing all the elements from the compression span.
				this->m_Chunks[index] = std::vector<std::byte>(compression_span.begin(), compression_span.begin() + csize);
				this->validate_chunk_sizes();
			}

			/// Append to the schunk with the uncompressed data (compressing it).
			///
			/// \param compressed the compressed chunk
			void append_chunk(std::vector<std::byte> compressed) override
			{
				this->m_Chunks.push_back(std::move(compressed));
				this->validate_chunk_sizes();
			};

			/// Append to the schunk with the uncompressed data (compressing it).
			///
			/// \param compression_ctx the compression context to use for compression.
			/// \param uncompressed the uncompressed chunk
			void append_chunk(blosc2::context_ptr& compression_ctx, std::span<T> uncompressed) override
			{
				util::default_init_vector<std::byte> compression_buffer(blosc2::min_compressed_size(this->chunk_bytes()));
				std::span<std::byte> compression_span(compression_buffer);
				this->append_chunk(compression_ctx, uncompressed, compression_span);
				this->validate_chunk_sizes();
			};

			void append_chunk(blosc2::context_ptr& compression_ctx, std::span<T> uncompressed, std::span<std::byte> compression_buff) override
			{
				if (compression_buff.size() < blosc2::min_compressed_size(this->chunk_bytes()))
				{
					throw std::runtime_error(
						std::format(
							"Error while appending chunk to super-chunk. Expected compression buffer to be at least"
							" {:L} bytes but instead we got {:L} bytes", blosc2::min_compressed_size(this->chunk_bytes()),
							compression_buff.size()
						)
					);
				}
				auto csize = blosc2::compress<T>(compression_ctx, uncompressed, compression_buff);
				assert(csize <= compression_buff.size());
				// copy over a new vector containing all the elements from the compression span.
				this->m_Chunks.push_back(std::vector<std::byte>(compression_buff.begin(), compression_buff.begin() + csize));
				this->validate_chunk_sizes();
			}

			size_t chunk_bytes(size_t index) const override
			{
				return blosc2::chunk_num_elements<T>(this->m_Chunks[index]) * sizeof(T);
			}

			/// The total compressed size of the schunk
			virtual size_t csize() const noexcept override
			{
				size_t _size = 0;
				for (const auto& chunk : this->m_Chunks)
				{
					_size += chunk.size();
				}
				return _size;
			};

			size_t size() const noexcept override
			{
				size_t _size = 0;
				for (const auto& chunk : this->m_Chunks)
				{
					_size += blosc2::chunk_num_elements<T>(chunk);
				}
				return _size;
			};

		};

	} // blosc2

} // NAMESPACE_COMPRESSED_IMAGE