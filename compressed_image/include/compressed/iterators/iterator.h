#pragma once

#include <ranges>
#include <vector>
#include <span>
#include <future>

#include "compressed/detail/scoped_timer.h"
#include "compressed/macros.h"
#include "compressed/blosc2_wrapper.h"
#include "compressed/containers/chunk_span.h"

namespace NAMESPACE_COMPRESSED_IMAGE
{

	// Image iterator, cannot be used in parallel as it iterates the chunks. Dereferencing it gives a span view over the current decompressed 
	// context.
	template <typename T, size_t ChunkSize = s_default_chunksize>
	struct channel_iterator
	{
		static constexpr size_t chunk_size = ChunkSize;

		// Iterator type definitions
		using iterator_category = std::forward_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = container::chunk_span<T, ChunkSize>;
		using pointer = value_type*;
		using reference = value_type&;

		channel_iterator() = default;

		channel_iterator(
			blosc2::schunk_raw_ptr schunk,
			blosc2::context_raw_ptr compression_context,
			blosc2::context_raw_ptr decompression_context,
			size_t chunk_index,
			size_t width,
			size_t height
			)
			: m_Schunk(schunk),
			m_CompressionContext(compression_context),
			m_DecompressionContext(decompression_context),
			m_ChunkIndex(chunk_index),
			m_Width(width),
			m_Height(height)
		{
			// Chunk size must be a multiple of the passed type.
			static_assert(ChunkSize % sizeof(T) == 0);

			// Check that we are not out of range, throw if we are
			if (static_cast<int64_t>(m_ChunkIndex) > m_Schunk->nchunks)
			{
				throw std::out_of_range(std::format(
					"chunk_index is out of range for total number of chunks in blosc2_schunk. Max chunk number is {} but received {}",
					m_Schunk->nchunks, m_ChunkIndex));
			}

			// Check that we don't pass zero width or height as e.g. the x() and y() functions of chunk_span require division by these dimensions
			if (m_Width == 0 || m_Height == 0)
			{
				throw std::runtime_error(
					std::format(
						"passed zero width or height to iterator which is not valid, expected at least 1 pixel in either dimensions. Got [width: {} px, height: {} px]",
						m_Width, m_Height
					)
				);
			}
		}

		~channel_iterator()
		{
			_COMPRESSED_PROFILE_FUNCTION();
			// We need to ensure that the last chunk also gets compressed on destruction
			// because of e.g. scope exit
			if (m_DecompressionBufferWasRefitted)
			{
				compress_chunk(m_CompressionContext);
				// If we iterated through the whole range at this point we'd have a
				// chunk index == nchunks but the last chunk was not yet compressed. In this case
				// we have to ensure we set the index back to compress again.
				auto chunk_idx = m_ChunkIndex;
				if (static_cast<int64_t>(m_ChunkIndex) == m_Schunk->nchunks)
				{
					chunk_idx = chunk_idx - 1;
				}
				update_chunk(m_Schunk, chunk_idx);
			}
		}

		/// Dereference operator: decompress the current chunk and recompress (if necessary) the previously compressed
		/// chunk. value_type is a view over the current buffers. Iterator going out of scope while value_type is accessed is UB.
		value_type operator*()
		{
			_COMPRESSED_PROFILE_FUNCTION();

			// Initialize the data, this allows the base iterator to be copied over
			// quite cheaply
			if (!m_Initialized)
			{
				m_CompressionBuffer.resize(blosc2::min_compressed_size<ChunkSize>());
				m_CompressionBufferSize = m_CompressionBuffer.size();
				m_DecompressionBuffer.resize(blosc2::min_decompressed_size<ChunkSize>());
				m_DecompressionBufferSize = m_DecompressionBuffer.size();
				m_Initialized = true;
			}

			if (!valid())
			{
				throw std::runtime_error("Invalid Iterator struct encountered, cannot dereference item");
			}

			// Compress the previously decompressed chunk if it has been modified.
			if (m_DecompressionBufferWasRefitted && m_ChunkIndex != 0)
			{
				compress_chunk(m_CompressionContext);
				update_chunk(m_Schunk, m_ChunkIndex - 1);
			}

			// In most cases m_Decompressed.fitted_data should be identical to m_Decompressed.data. However, this is not true
			// for the last chunk in the schunk which may not be the same decompressed size.
			decompress_chunk(m_DecompressionContext);

			if (decompression_buffer_byte_size() % sizeof(T) != 0)
			{
				throw std::runtime_error(
					std::format(
						"Unable to dereference iterator as the decompressed size is not a multiple of {}." \
						" Got {:L} bytes. This is likely an internal decompression error.",
						sizeof(T), decompression_buffer_byte_size()
					)
				);
			}

			std::span<T> item_span(reinterpret_cast<T*>(m_DecompressionBuffer.data()), m_DecompressionBufferSize / sizeof(T));
			return container::chunk_span<T, ChunkSize>(item_span, m_Width, m_Height, m_ChunkIndex);
		}

		// Pre-increment operator: move to the next chunk
		channel_iterator& operator++()
		{
			++m_ChunkIndex;
			if (static_cast<int64_t>(m_ChunkIndex) > m_Schunk->nchunks)
			{
				throw std::out_of_range("Iterator: count exceeds number of chunks");
			}
			return *this;
		}

		channel_iterator& operator++(int)
		{
			channel_iterator temp = *this;
			++(*this);
			return temp;
		}

		bool operator==(const channel_iterator& other) const noexcept
		{
			return m_ChunkIndex == other.m_ChunkIndex && m_Schunk == other.m_Schunk;
		}

		bool operator!=(const channel_iterator& other) const noexcept
		{
			return m_ChunkIndex != other.m_ChunkIndex || m_Schunk != other.m_Schunk;
		}

		/// Return the chunk index the iterator is currently at.
		size_t chunk_index() const noexcept { return m_ChunkIndex; }

	private:

		/// Buffers for storing compressed and decompressed data. These hold enough data for ChunkSize
		/// but may be smaller, thus we keep track of m_CompressionBufferSize and m_DecompressionBufferSize
		std::vector<std::byte>	m_CompressionBuffer;
		bool					m_CompressionBufferWasRefitted = false;
		size_t					m_CompressionBufferSize = 0;	// The fitted size of the container (only holding the compressed size)

		std::vector<std::byte>	m_DecompressionBuffer;
		bool					m_DecompressionBufferWasRefitted = false;
		size_t					m_DecompressionBufferSize = 0;	// The fitted size of the container (only holding the decompressed size)

		/// Pointers to the blosc2 structs. The data is owned by the `channel` struct and we just have a view over it.
		blosc2::schunk_raw_ptr	m_Schunk = nullptr;
		blosc2::context_raw_ptr m_CompressionContext = nullptr;
		blosc2::context_raw_ptr	m_DecompressionContext = nullptr;

		size_t	m_ChunkIndex = 0;
		size_t  m_Width = 0;
		size_t  m_Height = 0;

		/// this is set in the dereference operator to only initialize on first access
		/// not on setup.
		bool m_Initialized = false;

	private:

		size_t compression_buffer_byte_size() const noexcept
		{
			return m_CompressionBufferSize;
		}

		size_t compression_buffer_max_byte_size() const noexcept
		{
			return m_CompressionBuffer.size();
		}

		size_t decompression_buffer_byte_size() const noexcept
		{
			return m_DecompressionBufferSize;
		}

		size_t decompression_buffer_max_byte_size() const noexcept
		{
			return m_DecompressionBuffer.size();
		}

		/// Check for validity of this struct.
		bool valid() const
		{
			// Check that the schunk, compression and decompression ptrs are not null
			bool ptrs_valid = m_Schunk && m_CompressionContext && m_DecompressionContext;
			if (!ptrs_valid)
			{
				return false;
			}

			bool compression_size_valid = m_CompressionBufferSize <= m_CompressionBuffer.size();
			bool decompression_size_valid = m_DecompressionBufferSize <= m_DecompressionBuffer.size();

			bool idx_valid = static_cast<int64_t>(m_ChunkIndex) < m_Schunk->nchunks;
			bool compressed_data_valid = compression_buffer_max_byte_size() >= blosc2::min_compressed_size<ChunkSize>();
			bool decompressed_data_valid = decompression_buffer_max_byte_size() >= blosc2::min_decompressed_size<ChunkSize>();

			return idx_valid && compressed_data_valid && decompressed_data_valid && compression_size_valid && decompression_size_valid;
		}

		/// Decompress a chunk using the given context and chunk pointer. Decompressing into the buffer
		void decompress_chunk(blosc2::context_raw_ptr decompression_context_ptr)
		{
			_COMPRESSED_PROFILE_FUNCTION();
			auto chunk_span = std::span<std::byte>(reinterpret_cast<std::byte*>(m_Schunk->data[m_ChunkIndex]), 1);
			auto buffer_span = std::span<T>(reinterpret_cast<T*>(m_DecompressionBuffer.data()), m_DecompressionBufferSize / sizeof(T));
			auto decompressed_size = blosc2::decompress(decompression_context_ptr, buffer_span, chunk_span);

			if (decompressed_size % sizeof(T) != 0)
			{
				throw std::runtime_error(
					std::format(
						"Error while decompressing blosc2 chunk. Expected data to match up to a multiple of sizeof(T) ({}) but instead got {:L}",
						sizeof(T), decompressed_size
					)
				);
			}

			m_DecompressionBufferSize = static_cast<size_t>(decompressed_size);
			m_DecompressionBufferWasRefitted = true;
		}

		/// Compress a chunk from the decompressed view into the compressed view
		void compress_chunk(blosc2::context_raw_ptr compression_context_ptr)
		{
			_COMPRESSED_PROFILE_FUNCTION();
			std::span<T> fitted = { reinterpret_cast<T*>(m_DecompressionBuffer.data()), m_DecompressionBufferSize / sizeof(T) };
			auto compressed_size = blosc2::compress(compression_context_ptr, fitted, m_CompressionBuffer);
			
			m_CompressionBufferSize = compressed_size;
			m_CompressionBufferWasRefitted = true;
		}

		/// Update and replace the chunk inside of the superchunk at the given index.
		void update_chunk(blosc2::schunk_raw_ptr schunk, size_t chunk_index)
		{
			_COMPRESSED_PROFILE_FUNCTION();
			int64_t res = blosc2_schunk_update_chunk(schunk, chunk_index, reinterpret_cast<uint8_t*>(m_CompressionBuffer.data()), true);
			if (res < 0)
			{
				throw std::runtime_error(std::format("Error code {} while updating the blosc2 chunk in the super-chunk", res));
			}
		}
	};


} // NAMESPACE_COMPRESSED_IMAGE