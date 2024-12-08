#pragma once

#include "Macros.h"

#include <array>
#include <vector>
#include <span>
#include <memory>
#include <optional>
#include <limits>
#include <execution>

#include "blosc2.h"
#include "json.hpp"

#include "fwd.h"
#include "blosc2_wrapper.h"

using json_ordered = nlohmann::ordered_json;


namespace NAMESPACE_COMPRESSED_IMAGE 
{

	/// Compressed Image representation with easy access to different channels. Internally functions very similar to an NDArray
	/// with the important distinction that the number of dimensions is fixed to be 3-Dimensional (width, height, channels).
	/// The actual order of these in memory can be whatever you want, it may be multiple planar channels, interleaved or any other
	/// memory order. 
	/// 
	/// The image is stored in a non-resizable fashion so whatever the resolution was going into it, is what the image will be.
	/// To rescale or refit the image a new Image has to be constructed.
	/// 
	/// The data is compressed in memory and we store it as part of a blosc2 super-chunk which is essentially a 3d array of 
	/// super-chunk -> chunk -> block. Where having the block size fit into L1 cache and the Chunk size into L3 cache is desirable
	/// as each block can be handled by a single cpu core while the chunk fits well within shared L3 memory.
	/// 
	/// \tparam _Block_Size 
	///		The size of the blocks stored inside the chunks, defaults to 32KB which is enough to comfortably fit into the L1 cache
	///		of most modern CPUs. If you know your cpu can handle larger blocks feel free to up this number.
	/// 
	/// \tparam _Chunk_Size 
	///		The size of each individual chunk, defaults to 4MB which is enough to hold a 2048x2048 channel. This should be tweaked
	///		to be no larger than the size of the usual images you are expecting to compress for optimal performance but this could be 
	///		upped which will give better compression ratios.
	/// 
	template <typename T, size_t _Block_Size = 32'768, size_t _Chunk_Size = 4'194'304>
	struct Image
	{

		// Image iterator, cannot be used in parallel as it iterates the chunks. Dereferencing it gives a 
		struct Iterator
		{
			// Iterator type definitions
			using iterator_category = std::forward_iterator_tag;
			using difference_type = std::ptrdiff_t;
			using value_type = std::span<T>;
			using pointer = value_type*;
			using reference = value_type&;


			Iterator(
				blosc2::schunk_raw_ptr schunk,
				impl::CompressionView<T>& decompression_buffer,
				impl::CompressionView<T>& compression_buffer,
				blosc2::context_raw_ptr compression_context, 
				blosc2::context_raw_ptr decompression_context,
				size_t chunk_index
			)
				: m_Schunk(schunk),
				m_Decompressed(decompression_buffer),
				m_Compressed(compression_buffer),
				m_CompressionContext(compression_context),
				m_DecompressionContext(decompression_context),
				m_ChunkIndex(chunk_index)
			{
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

				if (m_ChunkIndex > m_Schunk->nchunks)
				{
					throw std::out_of_range(std::format(
						"chunk_index is out of range for total number of chunks in blosc2_schunk. Max chunk number is {} but received {}",
						m_Schunk->nchunks, m_ChunkIndex));
				}
			}

			// Dereference operator: decompress the current chunk
			value_type operator*()
			{
				if (!valid())
				{
					throw std::runtime_error("Invalid Iterator struct encountered, cannot dereference item");
				}

				// Compress the previously decompressed chunk if it has been modified.
				if (m_Decompressed->was_refitted())
				{
					compress_chunk(m_CompressionContext, m_Decompressed, m_Compressed);
					update_chunk(m_Schunk, m_ChunkIndex, )
				}

				decompress_chunk(m_DecompressionContext, m_Decompressed, m_Compressed);
				return m_Decompressed.fitted_data;
			}

			// Pre-increment operator: move to the next chunk
			Iterator& operator++()
			{
				++m_ChunkIndex;
				return *this;
			}

			bool operator==(const Iterator& other) const noexcept
			{
				return m_ChunkIndex == other.m_ChunkIndex;
			}

			bool operator!=(const Iterator& other) const noexcept
			{
				return !(*this == other);
			}

		private:
			/// Buffer for compressing data, owned by the Image struct and at least _Chunk_Size + BLOSC2_MAX_OVERHEAD in size.
			impl::CompressionView<T> m_Compressed;

			/// Buffer to decompressed data, owned by the Image struct and at least _Chunk_Size in size;
			impl::CompressionView<T> m_Decompressed;

			blosc2::schunk_raw_ptr	m_Schunk				= nullptr;
			blosc2::context_raw_ptr m_CompressionContext	= nullptr;
			blosc2::context_raw_ptr	m_DecompressionContext	= nullptr;
			std::size_t	m_ChunkIndex						= 0;

		private:

			/// Check for validity of this struct.
			bool valid()
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
			constexpr std::size_t min_compressed_size()
			{
				return _Chunk_Size + BLOSC2_MAX_OVERHEAD;
			}

			/// Get the minimum size needed to store the decompressed data.
			constexpr std::size_t min_decompressed_size()
			{
				return _Chunk_Size;
			}

			/// Decompress a chunk using the given context and chunk pointer. Decompressing into the buffer
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
			void compress_chunk(blosc2::context_raw_ptr compression_context_ptr, const impl::CompressionView<const T>& decompressed, impl::CompressionView<T>& compressed)
			{
				int compressed_size =  blosc2_compress_ctx(
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

			void update_chunk(blosc2::schunk_raw_ptr schunk, std::size_t chunk_index, const impl::CompressionView<T>& compressed)
			{
				blosc2_schunk_update_chunk(schunk, chunk_index, compressed.fitted_data.data(), true);
			}
		};


		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		Image(
			std::vector<std::vector<const T>> channels,
			std::size_t width,
			std::size_t height,
			std::size_t num_channels = 1,
			std::vector<std::string> channel_names = {},
			codec compression_codec = codec::lz4,
			std::size_t compression_level = 5
		)
		{
			
		}

		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		Image(
			const std::span<const std::byte> data,
			std::size_t type_size,
			std::size_t width,
			std::size_t height,
			std::size_t num_channels = 1,
			std::vector<std::string> channel_names = {},
			memory_order mem_order = memory_order::interleaved,
			codec compression_codec = codec::lz4,
			std::size_t compression_level = 5
		)
		{

		}

		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		Image(
			const std::span<const T> data,
			std::size_t width,
			std::size_t height,
			std::size_t num_channels = 1,
			std::vector<std::string> channel_names = {},
			memory_order mem_order = memory_order::interleaved,
			codec compression_codec = codec::lz4,
			std::size_t compression_level = 5
		)
		{

		}


		/// Prints some statistics about the imagefile structure
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		void print_statistics()
		{
			if (!m_Schunk)
			{
				return;
			}

			std::cout << "Statistics for image buffer:" << std::endl;
			std::cout << " Width:             " << m_Width << std::endl;
			std::cout << " Height:            " << m_Height << std::endl;
			std::cout << " Channels:          " << m_NumChannels << std::endl;
			std::cout << " Channelnames:      " << m_ChannelNames << std::endl;
			std::cout << " --------------     " << std::endl;
			std::cout << " Compressed Size:   " << m_Schunk->cbytes << std::endl;
			std::cout << " Uncompressed Size: " << m_Schunk->nbytes << std::endl;
			std::cout << " Metadata:          " << "\n " << m_Metadata.dump(4) << std::endl;

		}


		// ---------------------------------------------------------------------------------------------------------------------
		// Iterators
		// ---------------------------------------------------------------------------------------------------------------------

		Iterator begin()
		{
			std::span<T> compressed_view(m_CompressionBuffer.data, m_CompressionBuffer.size());
			std::span<T> decompressed_view(m_DecompressionBuffer.data, m_DecompressionBuffer.size());

			return Iterator(
				m_Schunk.get(),
				impl::CompressionView(compressed_view),
				impl::CompressionView(decompressed_view),
				m_CompressionContext.get(),
				m_DecompressionContext.get(),
				0
			);
		}


		Iterator end()
		{
			std::span<T> compressed_view(m_CompressionBuffer.data, m_CompressionBuffer.size());
			std::span<T> decompressed_view(m_DecompressionBuffer.data, m_DecompressionBuffer.size());

			return Iterator(
				m_Schunk.get(),
				impl::CompressionView(compressed_view),
				impl::CompressionView(decompressed_view),
				m_CompressionContext.get(),
				m_DecompressionContext.get(),
				m_Schunk->nchunks - 1
			);
		}


		// ---------------------------------------------------------------------------------------------------------------------
		// Accessors
		// ---------------------------------------------------------------------------------------------------------------------

		/// Width of the Image
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		std::size_t width() const noexcept
		{
			return m_Width;
		}

		/// Height of the image
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		std::size_t height() const noexcept
		{
			return m_Height;
		}

		/// Total number of channels in the image
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		std::size_t num_channels() const noexcept
		{
			return m_NumChannels;
		}

		/// Names of the channels stored on the image, are stored in the same order as the logical indices. So if the channelnames
		/// are { "B", "G", "R" } accessing channel "R" would be index 2.
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		std::vector<std::string> channelnames() const noexcept
		{
			return m_ChannelNames;
		}


		/// Set the channelnames according to their logical indices, 
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		void channelnames(std::vector<std::string> _channelnames) 
		{
			if (_channelnames.size() != m_NumChannels)
			{
				throw std::invalid_argument(std::format(
					"Invalid number of arguments received for setting channelnames. Expected vector size to be exactly {} but instead got {}", 
					m_NumChannels, 
					_channelnames.size()
				).c_str()
				);
			}
			m_ChannelNames = _channelnames;
		}


		/// Arbitrary user metadata, not authored or managed by Image, it's up to the caller to handle what goes in and comes out
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		void metadata(const json_ordered& _metadata) noexcept
		{
			m_Metadata = _metadata;
		}

		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		json_ordered& metadata() noexcept
		{
			return m_Metadata
		}

		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		const json_ordered& metadata() const noexcept
		{
			return m_Metadata
		}

		/// Strides for each of the channels. Stored as the offsets into the original buffer required to 
		/// offset along width, height and channels. These are not byte offsets but rather logical index
		/// offsets. 
		/// If the image is 640 (width), 480 (height), 3 (channels), and stored interleaved, these would be:
		/// 
		/// {
		///		3,			// from e.g. width   0 - 1 
		///		640 * 3,	// from e.g. height  0 - 1
		///		1			// from e.g. channel 0 - 1
		/// };
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		void strides(std::size_t width_stride, std::size_t height_stride, std::size_t channel_stride)
		{
			validate_strides(width_stride, height_stride, channel_stride);

			m_Strides[0] = width_stride;
			m_Strides[1] = height_stride;
			m_Strides[2] = channel_stride;
		}

		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		void strides(std::array<std::size_t, 3> _strides)
		{
			validate_strides(_strides[0], _strides[1], _strides[2]);

			m_Strides[0] = _strides[0];
			m_Strides[1] = _strides[1];
			m_Strides[2] = _strides[2];
		}

		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		std::array<std::size_t, 3> strides() const noexcept
		{
			return m_Strides;
		}


		/// Retrieve a view to the compression context. In most cases users will not have to modify this.
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		blosc2::context_raw_ptr compression_context() { return m_CompressionContext.get(); }

		/// Retrieve a view to the decompression context. In most cases users will not have to modify this.
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		blosc2::context_raw_ptr decompression_context() { return m_DecompressionContext.get(); }
		

		/// Update the number of threads used internally by c-blosc2 for compression and decompression.
		/// This is automatically set when iterating through the images with compressed::for_each for example
		/// by specifying the compression codec.
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		void update_nthreads(std::size_t nthreads)
		{
			m_CompressionContext	= create_compression_context(nthreads);
			m_DecompressionContext	= create_decompression_context(nthreads);
		}


	private:
		/// The storage for the internal data, stored contiguously in a compressed data format
		blosc2::schunk_ptr m_Schunk		= nullptr;
		codec m_Codec					= codec::lz4;

		/// Buffers for storing compressed and decompressed data
		std::array<T, _Chunk_Size + BLOSC2_MAX_OVERHEAD>	m_CompressionBuffer;
		std::array<T, _Chunk_Size>							m_DecompressionBuffer;

		/// We store a compression and decompression context here to allow us to reuse them rather than having
		/// to reinitialize them on launch. May be nullptr;
		blosc2::context_ptr m_CompressionContext	= nullptr;
		blosc2::context_ptr m_DecompressionContext	= nullptr;

		/// Arbitrary user metadata, not authored or managed by us, it's up to the caller to handle what goes in and comes out
		json m_Metadata;

		/// Optional set of channelnames to associate to the channels. If not specified sensible defaults are chosen. For example,
		/// if 3 channels are provided we default to { "R", "G", "B" }
		std::vector<std::string> m_ChannelNames;

		/// The width of the image file
		std::size_t m_Width = 0;

		/// The height of the image file
		std::size_t m_Height = 0;

		/// The number of channels in the image file
		std::size_t m_NumChannels = 1;

		/// width, height, and channel strides respectively. These are not stored as 
		/// If the image is 640 (width), 480 (height), 3 (channels), and stored interleaved, these would be:
		/// 
		/// m_Strides = {3, 640 * 3, 1};
		/// 
		/// - The first value (`3`) indicates the stride to move one width step (e.g., from one pixel to the next along a row, considering 3 channels per pixel).
		/// - The second value (`640 * 3`) indicates the stride to move one height step (e.g., from one row to the next, considering the row width in bytes).
		/// - The third value (`1`) indicates the stride to move one channel step (e.g., from one channel component to the next within a pixel).
		/// 
		/// On the other hand, if this was planar it would look like this:
		/// 
		/// m_Strides = {1, 640, 640 * 480}
		std::array<std::size_t, 3> m_Strides;

	private:

		std::int8_t codec_to_blosc2(codec compcode)
		{
			switch compcode
			{
			case codec::blosclz:
				return BLOSC_BLOSCLZ;
			case codec:lz4:
				return BLOSC_LZ4;
			case codec::lz4hc:
				return BLOSC_LZ4HC;
			case codec::zlib:
				return BLOSC_ZLIB;
			case codec::zstd:
				return BLOSC_ZSTD;
			}
		}

		blosc2::context_ptr create_compression_context(std::size_t nthreads)
		{
			auto cparams = BLOSC2_CPARAMS_DEFAULTS;
			cparams.blocksize = _Block_Size;
			cparams.typesize  = sizeof(T);
			cparams.nthreads  = nthreads;
			cparams.schunk	  = m_Schunk.get();
			cparams.compcode  = codec_to_blosc2(m_Codec);
			
			return std::make_unique(blosc2_create_cctx(cparams));
		}

		blosc2::context_ptr create_decompression_context(std::size_t nthreads)
		{
			auto dparams = BLOSC2_DPARAMS_DEFAULTS;
			dparams.schunk   = m_Schunk.get();
			dparams.nthreads = nthreads;

			return std::make_unique(blosc2_create_dctx(dparams));
		}

		void validate_strides(std::size_t width_stride, std::size_t height_stride, std::size_t channel_stride) const
		{
			if (m_Strides[0] == 0 || m_Strides[1] == 0 || m_Strides[2] == 0)
			{
				throw std::invalid_argument("Strides cannot be zero.");
			}
		}
	};

} // NAMESPACE_COMPRESSED_IMAGE