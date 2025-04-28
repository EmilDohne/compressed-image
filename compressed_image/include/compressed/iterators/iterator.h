#pragma once

#include <ranges>
#include <vector>
#include <span>
#include <future>

#include "compressed/detail/scoped_timer.h"
#include "compressed/macros.h"
#include "compressed/blosc2/wrapper.h"
#include "compressed/containers/chunk_span.h"

namespace NAMESPACE_COMPRESSED_IMAGE
{

	// Image iterator, cannot be used in parallel as it iterates the chunks. Dereferencing it gives a span view over the current decompressed 
	// context.
	template <typename T>
	struct channel_iterator
	{
		// Iterator type definitions
		using iterator_category = std::forward_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = container::chunk_span<T>;
		using pointer = value_type*;
		using reference = value_type&;

		channel_iterator() = default;

		channel_iterator(
			blosc2::schunk_var_ptr<T> schunk,
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
			// Check that we are not out of range, throw if we are
			std::visit([&](auto& schunk)
				{
					if (m_ChunkIndex > schunk.num_chunks())
					{
						throw std::out_of_range(
							std::format(
								"chunk_index is out of range for total number of chunks in blosc2_schunk."
								" Max chunk number is {} but received {}",
							schunk.num_chunks(), m_ChunkIndex
							)
						);
					}
				}, *m_Schunk);
			
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
				std::visit([&](auto& schunk)
					{
						if (static_cast<int64_t>(m_ChunkIndex) == schunk.num_chunks())
						{
							chunk_idx = chunk_idx - 1;
						}
					}, *m_Schunk);
				update_chunk(chunk_idx);
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
				m_CompressionBuffer.resize(blosc2::min_compressed_size(this->chunk_bytes()));
				m_CompressionBufferSize = m_CompressionBuffer.size();
				m_DecompressionBuffer.resize(blosc2::min_decompressed_size(this->chunk_bytes()));
				m_DecompressionBufferSize = m_DecompressionBuffer.size();
				m_Initialized = true;
			}

			if (!this->valid())
			{
				throw std::runtime_error("Invalid Iterator struct encountered, cannot dereference item");
			}

			// Compress the previously decompressed chunk if it has been modified.
			if (m_DecompressionBufferWasRefitted && m_ChunkIndex != 0)
			{
				this->compress_chunk(m_CompressionContext);
				this->update_chunk(m_ChunkIndex - 1);
			}

			// In most cases m_Decompressed.fitted_data should be identical to m_Decompressed.data. However, this is not true
			// for the last chunk in the schunk which may not be the same decompressed size.
			this->decompress_chunk(m_DecompressionContext);

			if (this->decompression_buffer_byte_size() % sizeof(T) != 0)
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
			return container::chunk_span<T>(item_span, m_Width, m_Height, m_ChunkIndex, this->chunk_bytes());
		}

		// Pre-increment operator: move to the next chunk
		channel_iterator& operator++()
		{
			++m_ChunkIndex;
			std::visit([&](auto& schunk)
				{
					if (m_ChunkIndex > schunk.num_chunks())
					{
						throw std::out_of_range("Iterator: count exceeds number of chunks");
					};
				}, *m_Schunk);
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

		/// Return the chunk size of all but the last chunk.
		size_t chunk_elements() const noexcept
		{
			return std::visit([&](auto& schunk) -> size_t
				{
					return schunk.chunk_elements();
				}, *m_Schunk);
		}

		/// Return the chunk size of all but the last chunk.
		size_t chunk_bytes() const noexcept
		{
			return std::visit([&](auto& schunk) -> size_t
				{
					return schunk.chunk_bytes();
				}, *m_Schunk);
		}

	private:

		/// Buffers for storing compressed and decompressed data. These hold enough data for ChunkSize
		/// but may be smaller, thus we keep track of m_CompressionBufferSize and m_DecompressionBufferSize
		util::default_init_vector<std::byte> m_CompressionBuffer;
		bool m_CompressionBufferWasRefitted = false;
		size_t m_CompressionBufferSize = 0;	// The fitted size of the container (only holding the compressed size)

		std::vector<std::byte> m_DecompressionBuffer;
		bool m_DecompressionBufferWasRefitted = false;
		size_t m_DecompressionBufferSize = 0;	// The fitted size of the container (only holding the decompressed size)

		/// Pointers to the blosc2 structs. The data is owned by the `channel` struct and we just have a view over it.
		blosc2::schunk_var_ptr<T> m_Schunk;
		blosc2::context_raw_ptr m_CompressionContext = nullptr;
		blosc2::context_raw_ptr	m_DecompressionContext = nullptr;

		size_t m_ChunkIndex = 0;
		size_t m_Width = 0;
		size_t m_Height = 0;

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
			if (!m_Schunk)
			{
				return false;
			}
			return std::visit([&](auto& schunk)
				{
					// Check that the schunk, compression and decompression ptrs are not null
					bool ptrs_valid = m_Schunk && m_CompressionContext && m_DecompressionContext;
					if (!ptrs_valid)
					{
						return false;
					}

					bool compression_size_valid = m_CompressionBufferSize <= m_CompressionBuffer.size();
					bool decompression_size_valid = m_DecompressionBufferSize <= m_DecompressionBuffer.size();

					bool idx_valid = static_cast<int64_t>(m_ChunkIndex) < schunk.num_chunks();
					bool compressed_data_valid = compression_buffer_max_byte_size() >= blosc2::min_compressed_size(this->chunk_bytes());
					bool decompressed_data_valid = decompression_buffer_max_byte_size() >= blosc2::min_decompressed_size(this->chunk_bytes());

					return idx_valid && compressed_data_valid && decompressed_data_valid && compression_size_valid && decompression_size_valid;
				}, *m_Schunk);
		}

		/// Decompress a chunk using the given context and chunk pointer. Decompressing into the buffer
		void decompress_chunk(blosc2::context_raw_ptr decompression_context_ptr)
		{
			_COMPRESSED_PROFILE_FUNCTION();
			auto buffer_span = std::span<T>(reinterpret_cast<T*>(m_DecompressionBuffer.data()), m_DecompressionBufferSize / sizeof(T));
			
			// apply the decompression.
			std::visit([&](auto& schunk) 
				{
					schunk.chunk(decompression_context_ptr, buffer_span, m_ChunkIndex);
					m_DecompressionBufferSize = schunk.chunk_bytes(m_ChunkIndex);
					m_DecompressionBufferWasRefitted = true;
				}, *m_Schunk);
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
		void update_chunk(size_t chunk_index)
		{
			_COMPRESSED_PROFILE_FUNCTION();
			auto byte_span = std::span<std::byte>(m_CompressionBuffer.data(), this->compression_buffer_byte_size());
			std::visit([&](auto& schunk)
				{
					schunk.set_chunk(byte_span, chunk_index);
				}, *m_Schunk);
		}
	};


} // NAMESPACE_COMPRESSED_IMAGE