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
#include "blosc2/wrapper.h"
#include "blosc2/typedefs.h"
#include "blosc2/schunk.h"
#include "blosc2/lazyschunk.h"
#include "constants.h"
#include "util.h"
#include "json_alias.h"
#include "detail/scoped_timer.h"
#include "iterators/iterator.h"


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
	template <typename T>
	struct channel : public std::ranges::view_interface<channel<T>>
	{
		using value_type = T;
		using iterator = channel_iterator<T>;
		using const_iterator = channel_iterator<const T>;

		channel(channel&& other)
		{
			m_Schunk = std::move(other.m_Schunk);
			m_Codec = other.m_Codec;
			m_Nthreads = other.m_Nthreads;			
			m_CompressionContext = std::move(other.m_CompressionContext);
			m_DecompressionContext = std::move(other.m_DecompressionContext);
			m_CompressionLevel = other.m_CompressionLevel;
			m_Width = other.m_Width;
			m_Height = other.m_Height;
		};
		channel& operator=(channel&& other)
		{
			if (this != &other)
			{
				m_Schunk = std::move(other.m_Schunk);
				m_Codec = other.m_Codec;
				m_Nthreads = other.m_Nthreads;
				m_CompressionContext = std::move(other.m_CompressionContext);
				m_DecompressionContext = std::move(other.m_DecompressionContext);
				m_CompressionLevel = other.m_CompressionLevel;
				m_Width = other.m_Width;
				m_Height = other.m_Height;
			}
			return *this;
		};
		channel(const channel&) = delete;
		channel& operator=(const channel&) = delete;
			

		/// Default ctor, ensures the schunk and compression/decompression contexts are always initialized
		/// into valid states. This will not generate a valid channel however and the ctor taking data or the static
		/// functions `zeros` and `full` are preferred.
		channel()
		{
			m_Schunk = std::make_shared<blosc2::schunk_var<T>>(blosc2::lazy_schunk<T>(0, 1, s_default_blocksize, s_default_chunksize));
			m_CompressionContext = blosc2::create_compression_context<T>(
				std::thread::hardware_concurrency() / 2,
				enums::codec::lz4,
				9,
				s_default_blocksize
			);
			m_DecompressionContext = blosc2::create_decompression_context(std::thread::hardware_concurrency() / 2);
		};

		/// Initialize the channel with the given data.
		/// 
		/// \param data The span of input data to be compressed.
		/// \param width The width of the image channel.
		/// \param height The height of the image channel.
		/// \param compression_codec The compression codec to be used.
		/// \param compression_level The compression level (default is 5).
		/// \param block_size The size of the blocks stored inside the chunks, defaults to 32KB which is enough to 
		///					  comfortably fit into the L1 cache of most modern CPUs. If you know your cpu can handle 
		///					  larger blocks feel free to up this number.
		/// \param chunk_size The size of each individual chunk, defaults to 4MB which is enough to hold a 2048x2048 channel. 
		///					  This should be tweaked to be no larger than the size of the usual images you are expecting  
		///					  to compress for optimal performance but this could be upped which might give better compression
		///					  ratios. Must be a multiple of sizeof(T).
		channel(
			const std::span<const T> data,
			size_t width,
			size_t height,
			enums::codec compression_codec = enums::codec::lz4,
			uint8_t compression_level = 9,
			size_t block_size = s_default_blocksize,
			size_t chunk_size = s_default_chunksize
		)
		{
			_COMPRESSED_PROFILE_FUNCTION();
			m_Width = width;
			m_Height = height;
			m_Codec = compression_codec;
			m_CompressionLevel = util::ensure_compression_level(compression_level);
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
			assert(chunk_size < std::numeric_limits<int32_t>::max());
			assert(block_size < chunk_size);

			m_CompressionContext = blosc2::create_compression_context<T>(std::thread::hardware_concurrency() / 2, m_Codec, m_CompressionLevel, block_size);
			m_DecompressionContext = blosc2::create_decompression_context(std::thread::hardware_concurrency() / 2);

			// Align the chunks to the scanlines, makes our lifes a lot easier on read/write.
			auto chunk_size_aligned = util::align_chunk_to_scanlines_bytes<T>(m_Width, chunk_size);
			m_Schunk = std::make_shared<blosc2::schunk_var<T>>(blosc2::schunk<T>(data, block_size, chunk_size_aligned, m_CompressionContext));
		}


		/// Initialize the channel with the given data.
		/// 
		/// \param schunk The initialized super-chunk.
		/// \param width The width of the image channel.
		/// \param height The height of the image channel.
		/// \param compression_codec The compression codec to be used.
		/// \param compression_level The compression level (default is 5).
		/// \param block_size The size of the blocks stored inside the chunks, defaults to 32KB which is enough to 
		///					  comfortably fit into the L1 cache of most modern CPUs. If you know your cpu can handle 
		///					  larger blocks feel free to up this number.
		/// \param chunk_size The size of each individual chunk, defaults to 4MB which is enough to hold a 2048x2048 channel. 
		///					  This should be tweaked to be no larger than the size of the usual images you are expecting  
		///					  to compress for optimal performance but this could be upped which might give better compression
		///					  ratios. Must be a multiple of sizeof(T).
		channel(
			blosc2::schunk_var<T> schunk,
			size_t width,
			size_t height,
			enums::codec compression_codec = enums::codec::lz4,
			uint8_t compression_level = 9
		)
		{
			_COMPRESSED_PROFILE_FUNCTION();
			m_Codec = compression_codec;
			m_CompressionLevel = util::ensure_compression_level(compression_level);

			if (std::holds_alternative<blosc2::schunk<T>>(schunk))
			{
				if (std::get<blosc2::schunk<T>>(schunk).size() != width * height)
				{
					throw std::invalid_argument(
						std::format(
							"Invalid schunk passed to compressed::channel constructor. Expected a size of {:L} but instead got {:L}",
							width * height,
							std::get<blosc2::schunk<T>>(schunk).size()
						)
					);
				}
			}
			else if (std::holds_alternative<blosc2::lazy_schunk<T>>(schunk))
			{
				if (std::get<blosc2::lazy_schunk<T>>(schunk).size() != width * height)
				{
					throw std::invalid_argument(
						std::format(
							"Invalid schunk passed to compressed::channel constructor. Expected a size of {:L} but instead got {:L}",
							width * height,
							std::get<blosc2::schunk<T>>(schunk).size()
						)
					);
				}
			}

			m_Schunk = std::make_shared<blosc2::schunk_var<T>>(std::move(schunk));
			m_Width = width;
			m_Height = height;

			// Store the compression and decompression contexts, retrieving the block size from the underlying schunk
			// wrapper
			std::visit([&](auto& schunk)
				{
					m_CompressionContext = blosc2::create_compression_context<T>(std::thread::hardware_concurrency() / 2, m_Codec, m_CompressionLevel, schunk.max_block_size());
					m_DecompressionContext = blosc2::create_decompression_context(std::thread::hardware_concurrency() / 2);
				}, *m_Schunk);
			
		}


		/// Create a channel filled with zeros.
		///
		/// Generates a lazy-channel which only stores a single value T per-chunk, only setting this to a compressed buffer
		/// if set with something like `set_chunk`. This is especially memory efficient and should be the preferred way 
		/// when wanting to generate an empty channel only filling out some parts (i.e. sparse cryptomatte loading).
		/// 
		/// \param width The width of the image channel.
		/// \param height The height of the image channel.
		/// \param compression_codec The compression codec to be used.
		/// \param compression_level The compression level (default is 9).
		/// \param block_size The size of the blocks stored inside the chunks, defaults to 32KB which is enough to 
		///                   comfortably fit into the L1 cache of most modern CPUs.
		/// \param chunk_size The size of each individual chunk, defaults to 4MB. Should be no larger than the expected image size 
		///                   for optimal performance and must be a multiple of sizeof(T).
		/// \return A channel instance with all values initialized to zero.
		static channel zeros(
			size_t width, 
			size_t height,
			enums::codec compression_codec = enums::codec::lz4,
			uint8_t compression_level = 9,
			size_t block_size = s_default_blocksize,
			size_t chunk_size = s_default_chunksize
		)
		{
			return channel<T>::full(width, height, static_cast<T>(0), compression_codec, compression_level, block_size, chunk_size);
		}

		/// Create a zero-initialized channel with the same shape and compression parameters as another channel.
		///
		/// Generates a lazy-channel which only stores a single value T per-chunk, only setting this to a compressed buffer
		/// if set with something like `set_chunk`. This is especially memory efficient and should be the preferred way 
		/// when wanting to generate an empty channel only filling out some parts (i.e. sparse cryptomatte loading).
		/// 
		/// \param other The reference channel from which to copy shape and compression settings.
		/// \return A new channel instance with the same dimensions and compression settings as \p other, filled with zeros.
		static channel zeros_like(const channel& other)
		{
			return channel<T>::zeros(
				other.width(), 
				other.height(), 
				other.compression(), 
				other.compression_level(), 
				other.block_size(), 
				other.chunk_size()
			);
		}

		/// Create a channel filled with a specific value.
		///
		/// Generates a lazy-channel which only stores a single value T per-chunk, only setting this to a compressed buffer
		/// if set with something like `set_chunk`. This is especially memory efficient and should be the preferred way 
		/// when wanting to generate an empty channel only filling out some parts (i.e. sparse cryptomatte loading).
		/// 
		/// \param width The width of the image channel.
		/// \param height The height of the image channel.
		/// \param fill_value The value to fill the channel with.
		/// \param compression_codec The compression codec to be used.
		/// \param compression_level The compression level (default is 9).
		/// \param block_size The size of the blocks stored inside the chunks, defaults to 32KB.
		/// \param chunk_size The size of each individual chunk, defaults to 4MB. Should be no larger than the expected image size 
		///                   for optimal performance and must be a multiple of sizeof(T).
		/// \return A channel instance with all values initialized to \p fill_value.
		static channel full(
			size_t width,
			size_t height,
			T fill_value,
			enums::codec compression_codec = enums::codec::lz4,
			uint8_t compression_level = 9,
			size_t block_size = s_default_blocksize,
			size_t chunk_size = s_default_chunksize
		)
		{
			const size_t chunk_size_aligned = util::align_chunk_to_scanlines_bytes<T>(width, chunk_size);
			const size_t num_elements = width * height;

			auto schunk = blosc2::lazy_schunk<T>(fill_value, num_elements, block_size, chunk_size_aligned);
			return channel(std::move(schunk), width, height, compression_codec, compression_level);
		}


		/// Create a channel filled with a specific value and the same shape and compression settings as another channel.
		///
		/// Generates a lazy-channel which only stores a single value T per-chunk, only setting this to a compressed buffer
		/// if set with something like `set_chunk`. This is especially memory efficient and should be the preferred way 
		/// when wanting to generate an empty channel only filling out some parts (i.e. sparse cryptomatte loading).
		/// 
		/// \param other The reference channel from which to copy shape and compression settings.
		/// \param fill_value The value to fill the channel with.
		/// \return A new channel instance filled with \p fill_value and the same dimensions and compression settings as \p other.
		static channel full_like(const channel& other, T fill_value)
		{
			return channel<T>::full(
				other.width(),
				other.height(),
				fill_value,
				other.compression(),
				other.compression_level(),
				other.block_size(),
				other.chunk_size()
			);
		}

		/// Returns an iterator pointing to the beginning of the compressed data.
		///
		/// \return An iterator to the beginning of the compressed data.
		iterator begin()
		{
			return iterator(m_Schunk, m_CompressionContext.get(), m_DecompressionContext.get(), 0, m_Width, m_Height);
		}

		/// Returns an iterator pointing to the end of the compressed data.
		///
		/// \return An iterator to the end of the compressed data.
		iterator end()
		{
			if (m_Schunk)
			{
				return std::visit([&](auto& schunk)
					{
						return iterator(m_Schunk, m_CompressionContext.get(), m_DecompressionContext.get(), schunk.num_chunks(), m_Width, m_Height);
					}, *m_Schunk);
			}
			throw std::runtime_error("Internal Error: Unable to create end iterator as m_Schunk is uninitialized.");
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
		/// \param block_size The block size to compress to
		void update_nthreads(size_t nthreads, size_t block_size = s_default_blocksize)
		{
			m_CompressionContext = blosc2::create_compression_context<T>(nthreads, m_Codec, m_CompressionLevel, block_size);
			m_DecompressionContext = blosc2::create_decompression_context(nthreads);
			m_Nthreads = nthreads;
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

		/// Retrieve the compression level used.
		///
		/// \return The compression level (typically from 1-9).
		size_t compression_level() const noexcept
		{
			return m_CompressionLevel;
		}

		/// Retrieve the compressed data size.
		///
		/// \return The size of the compressed data in bytes.
		size_t compressed_bytes() const
		{
			if (!m_Schunk)
			{
				throw std::runtime_error("Channel instance is not properly initialized, unable to get decompressed data");
			}

			if (std::holds_alternative<blosc2::schunk<T>>(*m_Schunk))
			{
				return std::get<blosc2::schunk<T>>(*m_Schunk).csize();
			}
			else if (std::holds_alternative<blosc2::lazy_schunk<T>>(*m_Schunk))
			{
				return std::get<blosc2::lazy_schunk<T>>(*m_Schunk).csize();
			}
			return {};
		}
		
		/// Retrieve the uncompressed data size.
		///
		/// \return The size of the uncompressed data in elements.
		size_t uncompressed_size() const
		{
			if (!m_Schunk)
			{
				throw std::runtime_error("Channel instance is not properly initialized, unable to get decompressed data");
			}

			if (std::holds_alternative<blosc2::schunk<T>>(*m_Schunk))
			{
				return std::get<blosc2::schunk<T>>(*m_Schunk).size();
			}
			else if (std::holds_alternative<blosc2::lazy_schunk<T>>(*m_Schunk))
			{
				return std::get<blosc2::lazy_schunk<T>>(*m_Schunk).size();
			}
			return {};
		}
		
		/// Retrieve the total number of chunks the channel stores.
		///
		/// \return The number of chunks.
		size_t num_chunks() const 
		{ 
			assert(m_Schunk != nullptr);

			if (std::holds_alternative<blosc2::schunk<T>>(*m_Schunk))
			{
				return std::get<blosc2::schunk<T>>(*m_Schunk).num_chunks();
			}
			else if (std::holds_alternative<blosc2::lazy_schunk<T>>(*m_Schunk))
			{
				return std::get<blosc2::lazy_schunk<T>>(*m_Schunk).num_chunks();
			}
			return {};
		}

		/// \brief Retrieve the block size (in bytes) of the channel
		///
		/// The internal blosc2 implementation reserves changing this value on compression so it may be possible
		/// that this is not the value you initially set.
		/// 
		/// \return The block size (in bytes).
		size_t block_size() const
		{
			assert(m_Schunk != nullptr);
			return std::visit([&](auto& schunk)
				{
					return schunk.max_block_size();
				}, *m_Schunk);
		}

		/// \brief Retrieve the chunk size (in bytes) of the channel
		/// 
		/// This will be all of the chunk sizes except for the last chunk. The last chunk may be smaller so to accurately
		/// capture it you should use the override with a size_t
		/// 
		/// \return The chunk size (in bytes).
		size_t chunk_size() const noexcept
		{
			assert(m_Schunk != nullptr);
			return std::visit([&](auto& schunk)
				{
					return schunk.chunk_bytes();
				}, *m_Schunk);
		}

		/// \brief Retrieve the chunk size (in bytes) of the channel at the given chunk index.
		/// 
		/// \return The chunk size (in bytes) at index `chunk_index`.
		/// 
		/// \throws std::out_of_range if the chunk index is invalid
		size_t chunk_size(size_t chunk_index) const
		{
			assert(m_Schunk != nullptr);
			return std::visit([&](auto& schunk)
				{
					return schunk.chunk_bytes(chunk_index);
				}, *m_Schunk);
		}


		/// Retrieves and decompresses a chunk of data into the provided buffer.
		///
		/// This function retrieves the chunk at the given index from the internal `schunk`,
		/// decompresses it using the current decompression context, and stores the result in `buffer`.
		///
		/// \param buffer A span representing the destination buffer to store the decompressed data.
		///               Must be large enough to hold one chunk of decompressed data.
		/// \param chunk_idx The index of the chunk to retrieve.
		///
		/// \throws std::runtime_error if the internal `schunk` pointer is not initialized.
		void get_chunk(std::span<T> buffer, size_t chunk_idx) const
		{
			if (!m_Schunk)
			{
				throw std::runtime_error("Internal Error: Channel instance is not properly initialized, unable to get decompressed data");
			}

			return std::visit([&](const auto& schunk)
				{
					// We cheat a little bit here by creating this compression ctx on the fly, unfortunately this is 
					// necessary as blosc2 will actually modify the ctx on decompression.
					auto decomp_ctx = blosc2::create_decompression_context(m_Nthreads);
					return schunk.chunk(decomp_ctx, buffer, chunk_idx);
				}, *m_Schunk);
		}

		/// Compresses and sets a chunk of data from the provided buffer at the specified index.
		///
		/// This function compresses the data in the provided buffer using the current compression
		/// context and writes it into the internal `schunk` at the given index.
		///
		/// \param buffer A span representing the source data to be compressed and stored.
		/// \param chunk_idx The index of the chunk to overwrite or set with the compressed data.
		///
		/// \throws std::runtime_error if the internal `schunk` pointer is not initialized.
		void set_chunk(std::span<T> buffer, size_t chunk_idx)
		{
			if (!m_Schunk)
			{
				throw std::runtime_error("Internal Error: Channel instance is not properly initialized, unable to set data");
			}

			return std::visit([&](auto& schunk)
				{
					return schunk.set_chunk(m_CompressionContext, buffer, chunk_idx);
				}, *m_Schunk);
		}

		/// Get the decompressed data as a vector.
		///
		/// \throws std::runtime_error if the internal `schunk` pointer is not initialized.
		/// 
		/// \return A vector containing the decompressed data.
		std::vector<T> get_decompressed() const
		{
			if (!m_Schunk)
			{
				throw std::runtime_error("Internal Error: Channel instance is not properly initialized, unable to get decompressed data");
			}
			return std::visit([&](const auto& schunk)
				{
					// We cheat a little bit here by creating this compression ctx on the fly, unfortunately this is 
					// necessary as blosc2 will actually modify the ctx on decompression.
					auto decomp_ctx = blosc2::create_decompression_context(m_Nthreads);
					return schunk.to_uncompressed(decomp_ctx);
				}, *m_Schunk);
		}

		/// Equality operators, compares pointers to check for equality
		bool operator==(const channel<T>& other) const noexcept
		{
			return this == &other;
		}

	private:
		/// The storage for the internal data, stored contiguously in a compressed data format
		blosc2::schunk_var_ptr<T> m_Schunk = nullptr;
		enums::codec m_Codec = enums::codec::lz4;

		size_t m_Nthreads = std::thread::hardware_concurrency() / 2;

		/// We store a compression and decompression context here to allow us to reuse them rather than having
		/// to reinitialize them on launch. May be nullptr;
		blosc2::context_ptr m_CompressionContext = nullptr;
		blosc2::context_ptr m_DecompressionContext = nullptr;

		/// Compression level.
		uint8_t m_CompressionLevel = 9;

		/// The width and height of the channel.
		size_t m_Width = 1;
		size_t m_Height = 1;
	};

} // NAMESPACE_COMPRESSED_IMAGE