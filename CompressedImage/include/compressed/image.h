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
#include "fwd.h"
#include "blosc2_wrapper.h"
#include "constants.h"

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
	template <typename T, std::size_t _Block_Size = s_default_blocksize, std::size_t _Chunk_Size = s_default_chunksize>
	struct image
	{
		static constexpr std::size_t block_size = _Block_Size;
		static constexpr std::size_t chunk_size = _Chunk_Size;


		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		image(
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
		image(
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
		image(
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

		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		iterator begin()
		{
			return iterator(m_Schunk.get(), m_CompressionContext.get(), m_DecompressionContext.get(), 0);
		}

		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		iterator end()
		{
			return iterator(m_Schunk.get(), m_CompressionContext.get(), m_DecompressionContext.get(), m_Schunk->nchunks);
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