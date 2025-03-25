#pragma once

#include <span>
#include <vector>
#include <byte>
#include <variant>

#include "macros.h"
#include "blosc2_wrapper.h"

namespace NAMESPACE_COMPRESSED_IMAGE
{

	namespace detail
	{
		/// Wrapper representing a lazy chunk holding either an initialized chunk
		/// in the form of a byte array or just a single T representing a lazy state
		template <typename T>
		struct lazy_chunk
		{
			std::variant<std::vector<std::byte>, T> value;
			size_t uncompressed_size = 0;
		};
	}


	template <typename T, size_t BlockSize = s_default_blocksize, size_t ChunkSize = s_default_chunksize>
	struct lazy_schunk
	{
		static constexpr size_t block_size = BlockSize;
		static constexpr size_t chunk_size = ChunkSize;

		/// Initialize a lazy super-chunk from the given value, has a near-zero
		/// cost with the chunks only being initialized on read/modify
		lazy_schunk(T value, size_t size)
		{
			size_t num_full_chunks = size / chunk_size;
			size_t remainder = size - (chunk_size * num_full_chunks);

			/// Initialize lazy chunks with the provided value of T
			for (auto idx : std::views::iota(num_full_chunks))
			{
				detail::lazy_chunk<T> chunk = { value, chunk_size };
				m_Chunks.push_back(std::move(chunk));
			}
			if (remainder > 0)
			{
				detail::lazy_chunk<T> chunk = { value, remainder};
				m_Chunks.push_back(std::move(chunk));
			}
		}

		/// convert the lazy schunk into a super-chunk, generating any 
		/// not yet initialized lazy chunks in the process. This should 
		/// be done once all the data is computed to minimize the overhead. 
		schunk_ptr to_schunk()
		{
			_COMPRESSED_PROFILE_FUNCTION();
			// Initialize the chunks, either appending the byte array directly to the schunk 
			blosc2::schunk_ptr schunk = create_default_schunk();

			// Allocate and compress the lazy buff 
			std::vector<std::byte> lazy_compressed_data;
			if (this->has_lazy_chunk())
			{
				auto lazy_buff = std::vector<T>(this->get_T_buffer_size(this->chunk_size, this->lazy_chunk_value()));
				lazy_compressed_data = std::vector<std::byte>(blosc2::min_compressed_size<chunk_size>());

				auto context = blosc2::create_compression_context(schunk, std::thread::hardware_concurrency(), enums::codec::lz4, 9);
				blosc2::compress(context, std::span<T>(lazy_buff), std::span<std::byte>(lazy_compressed_data));
			}

			// Iterate all the chunks, if lazy add the compressed lazy buffer, else add the compressed data.
			for (auto& chunk : m_Chunks)
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


		/// Retrieve the uncompressed chunk at `index`. Internally
		/// will either generate a lazy chunk or 
		std::vector<T> chunk(size_t index) const
		{
			if (index > m_Chunks.size() - 1)
			{
				throw std::out_of_range(
					std::format("Cannot access index {} in lazy-schunk. Total amount of chunks is {}", index, m_Chunks.size())
				);
			}


		}

		///
		void chunk(std::span<T> buffer, size_t index) const
		{

		}

	private:
		std::vector<detail::lazy_chunk<T>> m_Chunks{};

		/// Check whether m_Chunks contain any still-lazy chunks.
		bool has_lazy_chunk() const noexcept
		{
			for (const auto& chunk : m_Chunks)
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
			for (const auto& chunk : m_Chunks)
			{
				if (std::holds_alternative<T>(chunk.value))
				{
					return std::get<T>(chunk.value);
				}
			}
			return T{};

		}

		/// Get the buffer size for T for the given byte size. Checks that the buffer
		/// can be divided
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

}