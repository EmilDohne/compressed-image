#pragma once

#include <array>
#include <vector>
#include <span>
#include <memory>
#include <optional>
#include <limits>
#include <execution>

#include "blosc2.h"
#include "json.hpp"

#include "macros.h"
#include "enums.h"
#include "fwd.h"
#include "blosc2_wrapper.h"
#include "constants.h"

#include "iterators/iterator.h"

using json_ordered = nlohmann::ordered_json;


namespace NAMESPACE_COMPRESSED_IMAGE
{

	/// Compressed channel representation, usually stored as a part of a larger image. Allows for serial access while random
	/// access is limited. 
	/// 
	/// \code{.cpp}
	/// compressed::image<T> = ...;
	/// for (auto& chunk : image.channel_ref("r"))
	/// {
	///		for (auto& [index, pixel] : std::views::enumerate(chunk))
	///		{
	///			size_t x = chunk.x(index);
	///			size_t y = chunk.y(index);
	///			pixel = (x + y * image.width()) / image.size();
	///		}
	/// }
	/// \endcode
	/// 
	/// \tparam _Block_Size 
	///		The size of the blocks stored inside the chunks, defaults to 32KB which is enough to comfortably fit into the L1 cache
	///		of most modern CPUs. If you know your cpu can handle larger blocks feel free to up this number.
	/// 
	/// \tparam _Chunk_Size 
	///		The size of each individual chunk, defaults to 4MB which is enough to hold a 2048x2048 channel. This should be tweaked
	///		to be no larger than the size of the usual images you are expecting to compress for optimal performance but this could be 
	///		upped which might give better compression ratios.
	template <typename T, size_t BlockSize = s_default_blocksize, size_t ChunkSize = s_default_chunksize>
	struct channel : public std::ranges::view_interface<channel<T, BlockSize, ChunkSize>>
	{
		static constexpr size_t block_size = BlockSize;
		static constexpr size_t chunk_size = ChunkSize;

		channel() = default;
		~channel() = default;
		channel(const channel&) = delete;					// Delete copy constructor
		channel& operator=(const channel&) = delete;		// Delete copy assignment
		channel(channel&&) noexcept = default;				// Move constructor
		channel& operator=(channel&&) noexcept = default;	// Move assignment

		/// Initialize the channel with the given data.
		/// 
		/// \param data The span of input data to be compressed.
		/// \param width The width of the image channel.
		/// \param height The height of the image channel.
		/// \param compression_codec The compression codec to be used.
		/// \param compression_level The compression level (default is 5).
		channel(
			const std::span<const T> data,
			size_t width,
			size_t height,
			enums::codec compression_codec = enums::codec::lz4,
			uint8_t compression_level = 5
		) : m_Width(width), m_Height(height), m_Codec(compression_codec), m_CompressionLevel(compression_level)
		{
			if (data.size() != width * height)
			{
				throw std::runtime_error(
					std::format(
						"Invalid channel data passed. Expected its size to match up to width * height ({} * {}) which would be {:L}." \
						" Instead received {:L}",
						width, height, width * height, data.size()
					)
				);
			}

			// c-blosc2 chunks can at most be 2 gigabytes so the set chunk size should not exceed this.
			static_assert(ChunkSize < std::numeric_limits<int32_t>::max());
			static_assert(BlockSize < ChunkSize);

			// Initialize our Super-Chunk first as that is required for the compression and decompression contexts. 
			// The cparams and dparams don't matter as we use the verbose blosc2_schunk_append_chunk and blosc2_decompress_ctx
			auto cparams = BLOSC2_CPARAMS_DEFAULTS;
			auto dparams = BLOSC2_DPARAMS_DEFAULTS;
			blosc2_storage storage = { .cparams = &cparams, .dparams = &dparams };
			auto raw_schunk = blosc2_schunk_new(&storage);
			m_Schunk = blosc2::schunk_ptr(raw_schunk);

			m_CompressionContext = blosc2::create_compression_context<T, BlockSize>(m_Schunk, std::thread::hardware_concurrency(), compression_codec, compression_level);
			m_DecompressionContext = blosc2::create_decompression_context(m_Schunk, std::thread::hardware_concurrency());

			auto compression_span = std::span<const std::byte>(reinterpret_cast<const std::byte*>(data.data()), data.size() * sizeof(T));
			this->compress(compression_span);
		}


		/// Initialize the channel with the given data.
		/// 
		/// \param schunk The initialized super-chunk.
		/// \param width The width of the image channel.
		/// \param height The height of the image channel.
		/// \param compression_codec The compression codec to be used.
		/// \param compression_level The compression level (default is 5).
		channel(
			blosc2::schunk_ptr schunk,
			size_t width,
			size_t height,
			enums::codec compression_codec = enums::codec::lz4,
			uint8_t compression_level = 5
		) : m_Codec(compression_codec), m_CompressionLevel(compression_level)
		{
			// c-blosc2 chunks can at most be 2 gigabytes so the set chunk size should not exceed this.
			static_assert(ChunkSize < std::numeric_limits<int32_t>::max());
			static_assert(BlockSize < ChunkSize);

			if (schunk->nbytes != width * height * sizeof(T))
			{
				throw std::invalid_argument(
					std::format("Invalid schunk passed to compressed::channel constructor. Expected a size of {:L} but instead got {:L}",
						width * height * sizeof(T),
						schunk->nbytes)
				);
			}

			m_Schunk = std::move(schunk);
			m_Width = width;
			m_Height = height;

			m_CompressionContext = blosc2::create_compression_context<T, BlockSize>(m_Schunk, std::thread::hardware_concurrency(), compression_codec, compression_level);
			m_DecompressionContext = blosc2::create_decompression_context(m_Schunk, std::thread::hardware_concurrency());

		}

		/// Returns an iterator pointing to the beginning of the compressed data.
		///
		/// \return An iterator to the beginning of the compressed data.
		iterator<T, ChunkSize> begin()
		{
			return iterator<T, ChunkSize>(m_Schunk.get(), m_CompressionContext.get(), m_DecompressionContext.get(), 0, m_Width, m_Height);
		}

		/// Returns an iterator pointing to the end of the compressed data.
		///
		/// \return An iterator to the end of the compressed data.
		iterator<T, ChunkSize> end()
		{
			return iterator<T, ChunkSize>(m_Schunk.get(), m_CompressionContext.get(), m_DecompressionContext.get(), m_Schunk->nchunks, m_Width, m_Height);
		}

		/// Retrieve a view to the compression context. In most cases users will not have to modify this.
		///
		/// \return A pointer to the compression context.
		blosc2::context_raw_ptr compression_context() { return m_CompressionContext.get(); }

		/// Retrieve a view to the decompression context. In most cases users will not have to modify this.
		///
		/// \return A pointer to the decompression context.
		blosc2::context_raw_ptr decompression_context() { return m_DecompressionContext.get(); }

		/// Update the number of threads used internally by c-blosc2 for compression and decompression.
		/// 
		/// \param nthreads The number of threads to use for compression and decompression.
		void update_nthreads(size_t nthreads)
		{
			m_CompressionContext = blosc2::create_compression_context<T, BlockSize>(m_Schunk, nthreads, m_Codec, m_CompressionLevel);
			m_DecompressionContext = blosc2::create_decompression_context(m_Schunk, nthreads);
		}

		/// The channel width.
		///
		/// \return The width of the channel.
		size_t width() const noexcept { return m_Width; }

		/// The channel height.
		///
		/// \return The height of the channel.
		size_t height() const noexcept { return m_Height; }
		
		/// Retrieve the compression codec used.
		///
		/// \return The compression codec.
		enums::codec compression() const noexcept { return m_Codec; }

		/// Retrieve the compressed data size.
		///
		/// \return The size of the compressed data.
		size_t compressed_size() const noexcept { return m_Schunk->cbytes; }
		
		/// Retrieve the uncompressed data size.
		///
		/// \return The size of the uncompressed data.
		size_t uncompressed_size() const noexcept{ return m_Schunk->nbytes; }
		
		/// Retrieve the total number of chunks the channel stores.
		///
		/// \return The number of chunks.
		size_t num_chunks() const noexcept { return m_Schunk->nchunks; }

		/// Get the decompressed data as a vector.
		///
		/// \return A vector containing the decompressed data.
		std::vector<T> get_decompressed()
		{
			auto result = std::vector<T>(uncompressed_size() / sizeof(T));
			size_t offset = 0;
			for (auto chunk_index : std::views::iota(0, m_Schunk->nchunks))
			{
				auto buffer_size = std::min(ChunkSize / sizeof(T), result.size() - offset);
				auto sub_span = std::span<T>(result.data() + offset, buffer_size);
				// This span is a bit fake as we don't know the actual size of it but c-blosc will figure it out for us
				auto chunk_span = std::span<std::byte>(reinterpret_cast<std::byte*>(m_Schunk->data[chunk_index]), blosc2::min_compressed_size<ChunkSize>());

				auto decompressed_size = blosc2::decompress(m_DecompressionContext, sub_span, chunk_span);

				offset += decompressed_size / sizeof(T);
			}
			return result;
		}

		/// Equality operators, compares pointers to check for equality
		bool operator==(const channel<T, BlockSize, ChunkSize>& other) const noexcept
		{
			return this == &other;
		}

	private:
		/// The storage for the internal data, stored contiguously in a compressed data format
		blosc2::schunk_ptr m_Schunk = nullptr;
		enums::codec m_Codec = enums::codec::lz4;

		/// We store a compression and decompression context here to allow us to reuse them rather than having
		/// to reinitialize them on launch. May be nullptr;
		blosc2::context_ptr m_CompressionContext = nullptr;
		blosc2::context_ptr m_DecompressionContext = nullptr;

		/// Compression level.
		size_t m_CompressionLevel = 5;

		/// The width and height of the channel.
		size_t m_Width = 1;
		size_t m_Height = 1;
	private:

		/// Compress the image data into m_Schunk. Has to be called after initialization of m_Schunk
		/// as well as after the construction of m_CompressionContext.
		///
		/// \param data The span of data to compress.
		void compress(const std::span<const std::byte> data)
		{
			size_t orig_byte_size = data.size();
			size_t _num_chunks = std::ceil(static_cast<double>(orig_byte_size) / ChunkSize);

			// Allocate the chunk once for all of our compression routines.
			std::vector<std::byte> chunk(blosc2::min_compressed_size<ChunkSize>());
			
			size_t base_offset = 0;
			for (auto index : std::views::iota(static_cast<size_t>(0), _num_chunks))
			{
				// Get the size of the section to compress. If this is the last section
				// it may not align directly to the chunk boundary so we must compress less
				size_t section_size = ChunkSize;
				if (base_offset + section_size > orig_byte_size)
				{
					section_size = orig_byte_size - base_offset;
				}

				// Get a subspan of the current chunk to compress
				std::span<const std::byte> subspan(data.data() + base_offset, section_size);

				// We need to tell c-blosc2 that we want to insert at the end
				m_Schunk->current_nchunk = m_Schunk->nchunks;
				// Compress the context into a chunk which we then insert into blosc2
				// we check in the constructor that our chunk size does not exceed int32_t max.

				blosc2::compress(m_CompressionContext, subspan, chunk);
				blosc2::append_chunk(m_Schunk, chunk);

				// Increment our base offset to move onto the next chunk
				base_offset += section_size;
			}
		}
	};

} // NAMESPACE_COMPRESSED_IMAGE