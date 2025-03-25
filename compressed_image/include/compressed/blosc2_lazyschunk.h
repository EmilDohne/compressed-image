#pragma once

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
			std::vector<T> lazy_buff;

			schunk_ptr schunk = create_default_schunk();
			for (auto& chunk : m_Chunks)
			{
				if (std::holds_alternative<std::vector<std::byte>>(chunk.value))
				{
					auto& data = std::get<std::vector<std::byte>>(chunk.value);
				}
				else
				{
					auto data = std::get<T>(chunk.value);
					std::vector<T> vec(data, chunk.uncompressed_size);
				}
			}
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
	};

}