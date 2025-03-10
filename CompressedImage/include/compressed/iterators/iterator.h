#pragma once

#include "compressed/macros.h"
#include "compressed/blosc2_wrapper.h"
#include "compressed/strided_span.h"


namespace NAMESPACE_COMPRESSED_IMAGE
{

	// Image iterator, cannot be used in parallel as it iterates the chunks. Dereferencing it gives a span view over the current decompressed 
	// context.
	template <typename T, std::size_t _Chunk_Size = s_default_chunksize>
	struct iterator
	{
		static constexpr std::size_t chunk_size = _Chunk_Size;

		// Iterator type definitions
		using iterator_category = std::forward_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = strided_span<T>;
		using pointer = value_type*;
		using reference = value_type&;

		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		iterator(
			blosc2::schunk_raw_ptr schunk,
			blosc2::context_raw_ptr compression_context,
			blosc2::context_raw_ptr decompression_context,
			std::array<std::size_t, 3> strides,
			size_t chunk_index,

			)
			: m_Schunk(schunk),
			m_CompressionContext(compression_context),
			m_DecompressionContext(decompression_context),
			m_Strides(strides),
			m_ChunkIndex(chunk_index)
		{
			// Only initialize the memory if we are not initializing the end() iterator in which case this behaviour is unwanted as it would
			// allocate unnecessary extra memory.
			if (chunk_index != schunk->nchunks)
			{
				m_CompressionBuffer.resize(_Chunk_Size + BLOSC2_MAX_OVERHEAD);
				m_DecompressionBuffer.resize(_Chunk_Size);

				m_Compressed = impl::CompressionView<T>(m_CompressionBuffer.begin(), m_CompressionBuffer.end());
				m_Decompressed = impl::CompressionView<T>(m_DecompressionBuffer.begin(), m_DecompressionBuffer.end());

				// Sanity checks.
				if (m_Compressed.max_byte_size() < min_compressed_size())
				{
					throw std::length_error(std::format(
						"Invalid size for compression buffer passed. Expected at least {} but instead got {}",
						min_compressed_size(), m_Compressed.max_byte_size()));
				}

				if (m_Decompressed.max_byte_size() < min_decompressed_size())
				{
					throw std::length_error(std::format(
						"Invalid size for decompression buffer passed. Expected at least {} but instead got {}",
						min_decompressed_size(), m_Decompressed.max_byte_size()));
				}
			}

			if (m_ChunkIndex > m_Schunk->nchunks)
			{
				throw std::out_of_range(std::format(
					"chunk_index is out of range for total number of chunks in blosc2_schunk. Max chunk number is {} but received {}",
					m_Schunk->nchunks, m_ChunkIndex));
			}
		}

		// Dereference operator: decompress the current chunk and recompress (if necessary) the last channel.
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		value_type operator*()
		{
			if (!valid())
			{
				throw std::runtime_error("Invalid Iterator struct encountered, cannot dereference item");
			}

			// Compress the previously decompressed chunk if it has been modified.
			if (m_Decompressed->was_refitted() && m_ChunkIndex != 0)
			{
				compress_chunk(m_CompressionContext, m_Decompressed, m_Compressed);
				update_chunk(m_Schunk, m_ChunkIndex - 1, m_Compressed)
			}

			// In most cases m_Decompressed.fitted_data should be identical to m_Decompressed.data. However, this is not true
			// for the last chunk in the schunk
			decompress_chunk(m_DecompressionContext, m_Decompressed, m_Compressed);

			strided_span<T> out_container(m_Decompressed.fitted_data, 0, 1);
			return out_container;
		}

		// Pre-increment operator: move to the next chunk
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		iterator& operator++()
		{
			++m_ChunkIndex;
			return *this;
		}

		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		bool operator==(const iterator& other) const noexcept
		{
			return m_ChunkIndex == other.m_ChunkIndex;
		}

		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		bool operator!=(const iterator& other) const noexcept
		{
			return !(*this == other);
		}

		/// Return the chunk index the iterator is currently at.
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		std::size_t chunk_index() const noexcept { return m_ChunkIndex; }

	private:

		/// Buffers for storing compressed and decompressed data
		std::vector<T>	m_CompressionBuffer;
		std::vector<T>	m_DecompressionBuffer;

		/// Convenient views over m_CompressionBuffer and m_DecompressionBuffer.
		impl::CompressionView<T> m_Compressed;
		impl::CompressionView<T> m_Decompressed;

		blosc2::schunk_raw_ptr	m_Schunk = nullptr;
		blosc2::context_raw_ptr m_CompressionContext = nullptr;
		blosc2::context_raw_ptr	m_DecompressionContext = nullptr;
		std::size_t	m_ChunkIndex = 0;

		std::array<std::size_t, 3> m_Strides;

	private:

		/// Check for validity of this struct.
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		bool valid() const
		{
			bool ptrs_valid = m_Schunk && m_CompressionContext && m_DecompressionContext;
			if (!ptrs_valid)
			{
				return false;
			}

			bool idx_valid = m_ChunkIndex < m_Schunk->nchunks;
			bool compressed_data_valid = m_Compressed.max_byte_size() >= min_compressed_size();
			bool decompressed_data_valid = m_Decompressed.max_byte_size() >= min_decompressed_size();

			return idx_valid && compressed_data_valid && decompressed_data_valid;
		}

		/// Get the minimum size needed to store the compressed data.
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		constexpr std::size_t min_compressed_size()
		{
			return _Chunk_Size + BLOSC2_MAX_OVERHEAD;
		}

		/// Get the minimum size needed to store the decompressed data.
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		constexpr std::size_t min_decompressed_size()
		{
			return _Chunk_Size;
		}

		/// Decompress a chunk using the given context and chunk pointer. Decompressing into the buffer
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		void decompress_chunk(blosc2::context_raw_ptr decompression_context_ptr, impl::CompressionView<T>& decompressed, impl::CompressionView<const T>& compressed)
		{
			int decompressed_size = blosc2_decompress_ctx(
				decompression_context_ptr,
				compressed.data.data(),
				std::numeric_limits<int>::max(),
				decompressed.data.data(),
				decompressed.max_byte_size()
			);

			if (compressed_size < 0)
			{
				throw std::runtime_error("Error while compressing blosc2 chunk");
			}

			decompressed.refit(static_cast<std::size_t>(decompressed_size));
		}

		/// Compress a chunk from the decompressed view into the compressed view
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		void compress_chunk(blosc2::context_raw_ptr compression_context_ptr, const impl::CompressionView<const T>& decompressed, impl::CompressionView<T>& compressed)
		{
			int compressed_size = blosc2_compress_ctx(
				compression_context_ptr,
				decompressed.fitted_data.data(),
				decompressed.byte_size(),
				compressed.data.data(),
				compressed.max_byte_size(),
				);

			if (compressed_size < 0)
			{
				throw std::runtime_error("Error while compressing blosc2 chunk");
			}

			compressed.refit(static_cast<std::size_t>(compressed_size));
		}

		/// Update and replace the chunk inside of the superchunk at the given index.
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		void update_chunk(blosc2::schunk_raw_ptr schunk, std::size_t chunk_index, const impl::CompressionView<T>& compressed)
		{
			blosc2_schunk_update_chunk(schunk, chunk_index, compressed.fitted_data.data(), true);
		}
	};


} // NAMESPACE_COMPRESSED_IMAGE