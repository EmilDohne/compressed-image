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

using json_ordered = nlohmann::ordered_json;


namespace NAMESPACE_COMPRESSED_IMAGE 
{
	// forward declaration
	template <typename T, size_t _Block_Size = 32'768, size_t _Chunk_Size = 16'777'216>
	struct Image;


	/// Memory order of the image where `planar` is:
	/// 
	/// RRRR... GGGG... BBBB... AAAA...
	/// 
	/// and `interleaved` is 
	/// 
	/// RGBA RGBA RGBA RGBA...
	/// 
	/// When iterating a single channel at a time storing the images in a planar fashion might lead to faster results
	/// while for iterating multiple channels at a time it will make more sense to store the images interleaved.
	enum class memory_order
	{
		planar,
		interleaved
	};


	/// The compression codecs known to us. Inherited from blosc2.
	enum class codec
	{
		blosclz,
		lz4,
		lz4hc,
		zlib,
		zstd
	};



	namespace _Impl
	{

		// Custom deleter for blosc2 structs for use in a smart pointer
		template <typename T>
		struct Blosc2Deleter {};

		template <>
		struct Blosc2Deleter<blosc2_schunk>
		{
			void operator()(blosc2_schunk* schunk)
			{
				blosc2_schunk_free(schunk);
			}
		};

		template <>
		struct Blosc2Deleter<blosc2_context>
		{
			void operator()(blosc2_context* context)
			{
				blosc2_free_ctx(context);
			}
		};
	}


	/// Compressed Image representation with easy access to different channels. Internally functions very similar to an NDArray
	/// with the important distinction that the number of dimensions is fixed to be 3-Dimensional (width, height, channels).
	/// The actual order of these in memory can be whatever you want, it may be multiple planar channels, interleaved or any other
	/// memory order. 
	/// 
	/// The image is stored in a non-resizable fashion so whatever the resolution was going into it, is what the image will be.
	/// To rescale or refit the image a new Image has to be constructed.
	/// 
	/// To fully leverage the capabilities of the Image traversal should be done directly over the image as that will 
	/// 
	/// The data is compressed in memory and we store it as part of a blosc2 super-chunk which is essentially a 3d array of 
	/// super-chunk -> chunk -> block. Where having the block size fit into L1 cache and the Chunk size into L3 cache is desirable
	/// as each block can be handled by a single cpu core while the chunk fits well within shared L3 memory.
	/// 
	/// \tparam _Block_Size 
	///		The size of the blocks stored inside the chunks, defaults to 32KB which is enough to comfortably fit into the L1 cache
	///		of most modern CPUs. If you know your cpu can handle larger blocks feel free to up this.
	/// 
	/// \tparam _Chunk_Size 
	///		The size of each individual chunk, defaults to 16MB which is enough to hold a 4096x4096 image
	/// 
	template <typename T, size_t _Block_Size = 32'768, size_t _Chunk_Size = 16'777'216>
	struct Image
	{

		template <typename T, class Tag = void>
		class Iterator
			: public std::iterator<std::forward_iterator_tag, T>
		{
		public:

			T& operator*() const {};
			T* operator->() const {};

			Iterator& operator++() {};
			Iterator operator++(int) {};

			bool operator==(const Iterator& other) { };
			bool operator!=(const Iterator& other) { return !(*this == other); };

		private:
			Image& m_Parent;
			std::size_t m_Index;
			std::size_t m_CurrentChunkIndex;
		};


		/// Container class for iterating a specific channel
		struct ChannelContainer
		{

		};


		typedef Iterator<T>				iterator;
		typedef Iterator< const T>		const_iterator;


		using blosc2_schunk_ptr = std::unique_ptr<blosc2_schunk, _Impl::Blosc2Deleter<blosc2_schunk>>;
		using blosc2_chunk_ptr = std::byte*;
		using blosc2_context_ptr = std::unique_ptr<blosc2_context, _Impl::Blosc2Deleter<blosc2_context>>;
		
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
			if (!m_CompressedData)
			{
				return;
			}

			std::cout << "Statistics for image buffer:" << std::endl;
			std::cout << " Width:             " << m_Width << std::endl;
			std::cout << " Height:            " << m_Height << std::endl;
			std::cout << " Channels:          " << m_NumChannels << std::endl;
			std::cout << " Channelnames:      " << m_ChannelNames << std::endl;
			std::cout << " --------------     " << std::endl;
			std::cout << " Compressed Size:   " << m_CompressedData->cbytes << std::endl;
			std::cout << " Uncompressed Size: " << m_CompressedData->nbytes << std::endl;
			std::cout << " Metadata:          " << "\n " << m_Metadata.dump(4) << std::endl;

		}


		// ---------------------------------------------------------------------------------------------------------------------
		// Iterators
		// ---------------------------------------------------------------------------------------------------------------------


		/// Replacement implementation for the parallel for each 
		template<class ExecutionPolicy, class ForwardIt, class UnaryFunc>
			requires std::is_execution_policy_v<std::remove_reference_t<ExecutionPolicy>>
		constexpr UnaryFunc for_each(ExecutionPolicy&& _Exec, ForwardIt _First, ForwardIt _Last, UnaryFunc _Func)
		{
			std::for_each
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


	private:
		/// The storage for the internal data, stored contiguously in a compressed data format
		blosc2_schunk_ptr m_CompressedData		  = nullptr;

		/// We store a compression and decompression context here to allow us to reuse them rather than having
		/// to reinitialize them on launch. May be nullptr;
		blosc2_context_ptr m_CompressionContext   = nullptr;
		blosc2_context_ptr m_DecompressionContext = nullptr;

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

		/// Decompress a chunk using the given context and chunk pointer. Decompressing into the buffer
		blosc2_context_ptr decompress_chunk(blosc2_context_ptr decompression_context_ptr, std::span<T> preallocated_buffer, const blosc2_chunk_ptr chunk_ptr)
		{
			blosc2_decompress_ctx(
				decompression_context_ptr.get(),
				chunk_ptr,
				std::numeric_limits<int32_t>::max(),
				preallocated_buffer.data(),
				preallocated_buffer.size()
			);
		}

		/// Compress a chunk using the given context and chunk buffer. Compressing into the chunk buffer. Chunk buffer must be at least buffer.size() + BLOSC2_MAX_OVERHEAD
		blosc2_context_ptr compress_chunk(blosc2_context_ptr compression_context_ptr, const std::span<const T> buffer, std::span<T> chunk_buffer)
		{
			if (chunk_buffer.size() < buffer.size() + BLOSC2_MAX_OVERHEAD)
			{
				throw std::length_error(std::format("passed chunk_buffer must be at least of size buffer + BLOSC2_MAX_OVERHEAD. Expected {} but instead got {}", buffer.size() + BLOSC2_MAX_OVERHEAD, chunk_buffer.size())
			}

			blosc2_compress_ctx(
				compression_context_ptr.get(),
				preallocated_buffer.data(),
				preallocated_buffer.size(),
				chunk_buffer.data(),
				chunk_buffer.size(),
			);
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