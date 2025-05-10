#pragma once

#include <array>
#include <vector>
#include <span>
#include <memory>
#include <optional>
#include <limits>
#include <execution>
#include <tuple>
#include <filesystem>

#include <blosc2.h>
#include <json.hpp>

#ifdef COMPRESSED_IMAGE_OIIO_AVAILABLE
#include <OpenImageIO/imageio.h>
#endif

#include "macros.h"
#include "fwd.h"
#include "blosc2/wrapper.h"
#include "blosc2/schunk.h"
#include "blosc2/lazyschunk.h"
#include "constants.h"
#include "channel.h"
#include "image_algo.h"
#include "detail/oiio_util.h"

#include "iterators/iterator.h"

using json_ordered = nlohmann::ordered_json;


namespace NAMESPACE_COMPRESSED_IMAGE 
{

	/// Compressed Image representation with easy access to different channels. Internally functions very similar to an NDArray
	/// with the important distinction that the number of dimensions is fixed to be 3-Dimensional (width, height, channels).
	/// They are laid out in scanline order with each channel being its own distinct object which may have any size.
	/// 
	/// The image is stored in a non-resizable fashion so whatever the resolution was going into it, is what the image will be.
	/// To rescale or refit the image a new `image` has to be constructed.
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
	///		upped which might give better compression ratios. Must be a multiple of sizeof(T).
	template <typename T>
	struct image : public std::ranges::view_interface<image<T>>
	{

		image() = default;
		~image() = default;

		/// Constructs an image object with the specified channels, dimensions, and optional compression parameters.
		/// 
		/// This constructor creates an image from a given set of channels. The channel names can optionally be specified. 
		/// The image is then compressed using the provided codec and compression level.
		/// 
		/// Example:
		/// \code{.cpp}
		/// std::vector<std::vector<const uint8_t>> channels = ...;
		/// compressed::image<uint8_t> my_image(channels, 1920, 1080, {"r", "g", "b"}, codec::lz4, 5);
		/// \endcode
		/// 
		/// \param channels A vector of vectors containing the image channels (each channel is a 2D array of pixel data).
		/// \param width The width of the image in pixels.
		/// \param height The height of the image in pixels.
		/// \param channel_names (Optional) A list of channel names, must match the number of channels provided. 
		///					     If omitted or incorrect, channel names are ignored.
		/// \param compression_codec (Optional) The codec used for compression, default is `codec::lz4`.
		/// \param compression_level (Optional) The compression level, default is `9`.
		/// \param block_size The size of the blocks stored inside the chunks, defaults to 32KB which is enough to 
		///					  comfortably fit into the L1 cache of most modern CPUs. If you know your cpu can handle 
		///					  larger blocks feel free to up this number.
		/// \param chunk_size The size of each individual chunk, defaults to 4MB which is enough to hold a 2048x2048 channel. 
		///					  This should be tweaked to be no larger than the size of the usual images you are expecting  
		///					  to compress for optimal performance but this could be upped which might give better compression
		///					  ratios. Must be a multiple of sizeof(T).
		/// \throws std::runtime_error if a channel fails to be inserted.
		image(
			std::vector<std::vector<T>> channels,
			size_t width,
			size_t height,
			std::vector<std::string> channel_names = {},
			enums::codec compression_codec = enums::codec::lz4,
			size_t compression_level = 9,
			size_t block_size = s_default_blocksize,
			size_t chunk_size = s_default_chunksize
		)
		{
			m_Width = width;
			m_Height = height;
			m_ChannelNames = channel_names;
			auto comp_level_adjusted = util::ensure_compression_level(compression_level);

			// c-blosc2 chunks can at most be 2 gigabytes so the set chunk size should not exceed this.
			assert(chunk_size < std::numeric_limits<int32_t>::max());
			assert(block_size < chunk_size);
			if (channel_names.size() != channels.size() && channel_names.size() != 0)
			{
				std::cout << std::format(
					"Invalid channelnames passed to image constructor, required them to match the number of" \
					" channels in the channels parameter.Expected {} items but instead got {} names. Ignoring channel names", 
					channels.size(), channel_names.size()) << std::endl;

				m_ChannelNames = {};
			}

			// Iterate all channels and start creating channels for it.
			size_t channel_idx = 0;
			for (const auto& _channel : channels)
			{
				try
				{
					// Generate the channel and append it.
					m_Channels.push_back(compressed::channel<T>(
						std::span<const T>(_channel.begin(), _channel.end()),
						width,
						height,
						compression_codec,
						comp_level_adjusted,
						block_size,
						chunk_size
					));
				}
				catch (const std::exception& e)
				{
					if (m_ChannelNames.size() > 0)
					{
						throw std::runtime_error(
							std::format(
								"Failed to insert channel '{}' at position {}. Full error: \n{}",
								m_ChannelNames[channel_idx],
								channel_idx,
								e.what()
							)
						);
					}
					else
					{
						throw std::runtime_error(
							std::format(
								"Failed to insert channel at position {}. Full error: \n{}",
								channel_idx,
								e.what()
							)
						);
					}
				}
				++channel_idx;
			}
		}


		image(
			std::vector<compressed::channel<T>> channels,
			size_t width,
			size_t height,
			std::vector<std::string> channel_names = {}
		)
		{
			m_Width = width;
			m_Height = height;
			m_ChannelNames = channel_names;

			if (channel_names.size() != channels.size() && channel_names.size() != 0)
			{
				std::cout << std::format(
					"Invalid channelnames passed to image constructor, required them to match the number of" \
					" channels in the channels parameter.Expected {} items but instead got {} names. Ignoring channel names",
					channels.size(), channel_names.size()) << std::endl;

				m_ChannelNames = {};
			}
			m_Channels = std::move(channels);
		}


#ifdef COMPRESSED_IMAGE_OIIO_AVAILABLE

		/// \brief Reads a compressed image from a file using OpenImageIO and compresses it during reading.
		/// 
		/// Requires CompressedImage to have been compiled with OpenImageIO support.
		/// 
		/// This function reads an image file in chunks and compresses it on the fly leading to much
		/// lower memory usage at near-identical performance to raw OpenImageIO reads. On an image
		/// that is well compressible this can easily achieve a compression ratio of 5-10x.
		/// 
		/// The type does not have to match that of the underlying image as OpenImageIO will take
		/// care of converting the files into the specified format. It is perfectly valid to read 
		/// a floating point image as e.g. uint16_t etc.
		/// 
		/// Example:
		/// \code{.cpp}
		/// std::filesystem::path filepath = "image.exr";
		/// auto img = compressed::image::read<uint8_t>(filepath, compressed::enums::codec::lz4, 5);
		/// \endcode
		///
		/// \param filepath The file path of the image to read.
		/// \param compression_codec The compression codec to use (default: LZ4).
		/// \param compression_level The compression level (default: 9).
		/// \param block_size The size of the blocks stored inside the chunks, defaults to 32KB which is enough to 
		///					  comfortably fit into the L1 cache of most modern CPUs. If you know your cpu can handle 
		///					  larger blocks feel free to up this number.
		/// \param chunk_size The size of each individual chunk, defaults to 4MB which is enough to hold a 2048x2048 channel. 
		///					  This should be tweaked to be no larger than the size of the usual images you are expecting  
		///					  to compress for optimal performance but this could be upped which might give better compression
		///					  ratios. Must be a multiple of sizeof(T).
		/// \return A compressed image instance.
		static image read(
			std::filesystem::path filepath,
			enums::codec compression_codec = enums::codec::lz4,
			size_t compression_level = 9,
			size_t block_size = s_default_blocksize,
			size_t chunk_size = s_default_chunksize
		)
		{
			// Initialize the OIIO primitives
			auto input_ptr = OIIO::ImageInput::open(filepath);
			if (!input_ptr)
			{
				throw std::invalid_argument(std::format("File {} does not exist on disk", filepath.string()));
			}
			const OIIO::ImageSpec& spec = input_ptr->spec();

			return image<T>::read(
				std::move(input_ptr),
				spec.channelnames,
				compression_codec,
				compression_level,
				block_size,
				chunk_size
			);
		}


		/// \brief Reads a compressed image from a file using OpenImageIO and compresses it during reading.
		/// 
		/// Requires CompressedImage to have been compiled with OpenImageIO support.
		/// 
		/// This function reads an image file in chunks and compresses it on the fly leading to much
		/// lower memory usage at near-identical performance to raw OpenImageIO reads. On an image
		/// that is well compressible this can easily achieve a compression ratio of 5-10x.
		/// 
		/// The type does not have to match that of the underlying image as OpenImageIO will take
		/// care of converting the files into the specified format. It is perfectly valid to read 
		/// a floating point image as e.g. uint16_t etc.
		/// 
		/// This overload allows you to only extract the channels specified which is useful if you have e.g. 
		/// a multilayer file but only wish to extract the RGBA components.
		/// 
		/// We will internally take care of optimizing the calls to the OpenImageIO API for maximum read throughput.
		/// 
		/// Example:
		/// \code{.cpp}
		/// std::filesystem::path filepath = "image.exr";
		/// 
		/// auto input_ptr = OIIO::ImageInput::open(filepath);
		/// if (!input_ptr)
		/// {
		/// 	throw std::runtime_error(std::format("file {} does not exist on disk", filepath.string()));
		/// }
		/// 
		/// auto img = compressed::image::read<uint8_t>(input_ptr, {0, 1, 2, 3});
		/// \endcode
		///
		/// \param filepath The file path of the image to read.
		/// \param channelindices The channels you wish to extract. These may be specified in any order. We throw a 
		///						  std::out_of_range if one of the passed channels does not exist. It is perfectly valid
		///						  to e.g. call this with {3, 1, 2} when the underlying channel structure may be 
		///						  RGBA. Sorting these back into their underlying channel structure is done on read.
		/// \param compression_codec The compression codec to use (default: LZ4).
		/// \param compression_level The compression level (default: 9).
		/// \param block_size The size of the blocks stored inside the chunks, defaults to 32KB which is enough to 
		///					  comfortably fit into the L1 cache of most modern CPUs. If you know your cpu can handle 
		///					  larger blocks feel free to up this number.
		/// \param chunk_size The size of each individual chunk, defaults to 4MB which is enough to hold a 2048x2048 channel. 
		///					  This should be tweaked to be no larger than the size of the usual images you are expecting  
		///					  to compress for optimal performance but this could be upped which might give better compression
		///					  ratios. Must be a multiple of sizeof(T).
		/// \return A compressed image instance.
		static image read(
			std::unique_ptr<OIIO::ImageInput> input_ptr,
			std::vector<int> channelindices,
			enums::codec compression_codec = enums::codec::lz4,
			size_t compression_level = 9,
			size_t block_size = s_default_blocksize,
			size_t chunk_size = s_default_chunksize
		)
		{
			std::vector<std::string> channelnames{};
			const auto& spec = input_ptr->spec();

			for (int i : channelindices)
			{
				channelnames.push_back(spec.channelnames.at(i));
			}

			return image::read(
				std::move(input_ptr),
				channelnames,
				compression_codec,
				compression_level,
				block_size,
				chunk_size
			);
		}

		/// \brief Reads a compressed image from a file using OpenImageIO and compresses it during reading.
		/// 
		/// Requires CompressedImage to have been compiled with OpenImageIO support.
		/// 
		/// This function reads an image file in chunks and compresses it on the fly leading to much
		/// lower memory usage at near-identical performance to raw OpenImageIO reads. On an image
		/// that is well compressible this can easily achieve a compression ratio of 5-10x.
		/// 
		/// The type does not have to match that of the underlying image as OpenImageIO will take
		/// care of converting the files into the specified format. It is perfectly valid to read 
		/// a floating point image as e.g. uint16_t etc.
		/// 
		/// This overload allows you to only extract the channels specified which is useful if you have e.g. 
		/// a multilayer file but only wish to extract the RGBA components.
		/// 
		/// We will internally take care of optimizing the calls to the OpenImageIO API for maximum read throughput.
		/// 
		/// Example:
		/// \code{.cpp}
		/// std::filesystem::path filepath = "image.exr";
		/// 
		/// auto input_ptr = OIIO::ImageInput::open(filepath);
		/// if (!input_ptr)
		/// {
		/// 	throw std::runtime_error(std::format("file {} does not exist on disk", filepath.string()));
		/// }
		/// 
		/// auto img = compressed::image::read<uint8_t>(input_ptr, {"R", "G", "B", "A"});
		/// \endcode
		///
		/// \param filepath The file path of the image to read.
		/// \param channelnames The channels you wish to extract. These may be specified in any order. We throw a 
		///						std::out_of_range if one of the passed channels does not exist. It is perfectly valid
		///						to e.g. call this with {"G", "R", "A"} when the underlying channel structure may be 
		///						RGBA. Sorting these back into their underlying channel structure is done on read.
		/// \param compression_codec The compression codec to use (default: LZ4).
		/// \param compression_level The compression level (default: 9).
		/// \param block_size The size of the blocks stored inside the chunks, defaults to 32KB which is enough to 
		///					  comfortably fit into the L1 cache of most modern CPUs. If you know your cpu can handle 
		///					  larger blocks feel free to up this number.
		/// \param chunk_size The size of each individual chunk, defaults to 4MB which is enough to hold a 2048x2048 channel. 
		///					  This should be tweaked to be no larger than the size of the usual images you are expecting  
		///					  to compress for optimal performance but this could be upped which might give better compression
		///					  ratios. Must be a multiple of sizeof(T).
		/// \return A compressed image instance.
		static image read(
			std::unique_ptr<OIIO::ImageInput> input_ptr,
			std::vector<std::string> channelnames,
			enums::codec compression_codec = enums::codec::lz4,
			size_t compression_level = 9,
			size_t block_size = s_default_blocksize,
			size_t chunk_size = s_default_chunksize
		)
		{
			assert(chunk_size % sizeof(T) == 0);
			auto comp_level_adjusted = util::ensure_compression_level(compression_level);

			const OIIO::ImageSpec& spec = input_ptr->spec();
			const auto typedesc = enums::get_type_desc<T>();

			// Reject tiled files
			if (spec.tile_width != 0)
			{
				throw std::runtime_error("Opening tiled image files is currently unsupported.");
			}

			// Align the chunk size to the scanlines, this makes our life considerably easier and allows
			// us to not deal with partial scanlines.
			const size_t chunk_size_aligned = util::align_chunk_to_scanlines_bytes<T>(spec.width, chunk_size);

			// Get a std::vector containing a begin-end pair for all contiguous channels in our channelnames.
			// So if we pass 'R', 'B' and 'A' in a rgba image we would get the following
			// { {0 - 1}, {2 - 4} }
			// This allows us to both maximize performance by handling as many channels in one go as we can while also
			// minimizing memory footprint by only ever allocating as much as we need for the max amount of contiguous
			// channels we can encounter.
			std::vector<compressed::channel<T>> channels;
			auto channel_ranges_contiguous = detail::get_contiguous_channels(input_ptr, channelnames);
			size_t max_num_channels = 0;
			for (const auto& [chbegin, chend] : channel_ranges_contiguous)
			{
				if (static_cast<size_t>(chend) - chbegin > max_num_channels)
				{
					max_num_channels = static_cast<size_t>(chend) - chbegin;
				}
			}


			// Set up scratch buffers
			// -----------------------------------------------------------------------------------
			// -----------------------------------------------------------------------------------

			// Maximum chunk size we will need to account for (times number of channels).
			const size_t max_chunk_size = chunk_size_aligned * max_num_channels;

			// Initialize our swap buffers, these are going to be either discarded after
			// or compressed from.
			util::default_init_vector<T> interleaved_buffer(max_chunk_size / sizeof(T));
			std::vector<util::default_init_vector<T>> deinterleaved_buffer(max_num_channels);
			std::for_each(std::execution::par_unseq, deinterleaved_buffer.begin(), deinterleaved_buffer.end(), [&](auto& buffer)
				{
					buffer.resize(chunk_size_aligned / sizeof(T));
				});

			// Buffer to hold a single chunk. We will reuse this quite frequently
			auto chunk_buffer = util::default_init_vector<std::byte>(blosc2::min_compressed_size(chunk_size_aligned));

			// Read and compress the channel pairs in chunks
			// -----------------------------------------------------------------------------------
			// -----------------------------------------------------------------------------------

			// This will be the channelnames we will construct the image with. This is to avoid cases where the user
			// passes the channel names in a different order than they appear in such as 'A', 'G', 'R'. This should
			// still create the channel names as expected in correct order.
			std::vector<std::string> new_channelnames{};

			// Iterate all the pair and extract them, refitting the buffers as needed.
			// This is where the actual work of reading start. 
			for (auto [chbegin, chend] : channel_ranges_contiguous)
			{
				// Calculate some preliminary data for computing how many scanlines to extract in one go.
				int nchannels = chend - chbegin;
				const size_t bytes_per_scanline = static_cast<size_t>(spec.width) * nchannels * sizeof(T);

				const size_t chunk_size_all = chunk_size_aligned * nchannels;
				const size_t scanlines_per_chunk = chunk_size_all / bytes_per_scanline;

				// Refit the swap buffers as `read_contiguous_oiio_channels` expects these to be exactly sized.
				auto interleaved_fitted = std::span<T>(interleaved_buffer.begin(), chunk_size_all / sizeof(T));
				std::vector<std::span<T>> deinterleaved_fitted{};
				for (auto idx : std::views::iota(0, nchannels))
				{
					// construct a span from the util::default_init_vector
					deinterleaved_fitted.push_back(
						std::span<T>(deinterleaved_buffer.at(idx).begin(), deinterleaved_buffer.at(idx).end())
					);
				}

				// Create and initialize the contexts and schunks. These are pretty light weight so we don't need
				// to worry about creating them outside of the loop/reusing them.
				std::vector<blosc2::context_ptr> contexts;
				std::vector<blosc2::schunk<T>> schunks;
				for ([[maybe_unused]] auto _ : std::views::iota(0, nchannels))
				{
					schunks.push_back(blosc2::schunk<T>(block_size, chunk_size_aligned));
					contexts.push_back(blosc2::create_compression_context<T>(
						std::thread::hardware_concurrency(),
						compression_codec,
						comp_level_adjusted,
						block_size
					));
				}

				// Read the contiguous channel sequence into the contexts and schunks.
				read_contiguous_oiio_channels(
					input_ptr, 
					chbegin, 
					chend,
					interleaved_fitted,
					deinterleaved_fitted,
					scanlines_per_chunk,
					contexts,
					schunks,
					chunk_buffer
				);


				// Finally create the channels from the schunks
				for (const auto channel_idx : std::views::iota(0, nchannels))
				{
					channels.push_back(
						compressed::channel<T>(
							std::move(schunks[channel_idx]),
							spec.width,
							spec.height,
							compression_codec,
							comp_level_adjusted
						)
					);
				}
				// Store the correctly mapped channelnames
				for (auto channel_idx : std::views::iota(chbegin, chend))
				{
					new_channelnames.push_back(spec.channelnames.at(channel_idx));
				}
			}

			// Construct the image instance.
			return compressed::image<T>(std::move(channels), spec.width, spec.height, new_channelnames);
		}

#endif // COMPRESSED_IMAGE_OIIO_AVAILABLE

		/// Adds a compressed channel to the image.
		/// 
		/// This method moves the provided channel into the image's internal storage, adding it to the list of channels.
		/// 
		/// Example:
		/// \code{.cpp}
		/// compressed::channel<uint8_t, BlockSize, ChunkSize> channel = ...;
		/// my_image.add_channel(std::move(channel));
		/// \endcode
		/// 
		/// \param _channel The channel to be added to the image.
		/// \param name (Optional) Channel name of the channel to be inserted. If no channel names are set this argument is ignored.
		void add_channel(compressed::channel<T> _channel, std::optional<std::string> name = std::nullopt)
		{
			m_Channels.push_back(std::move(_channel));
			if (m_ChannelNames.size() > 0)
			{
				m_ChannelNames.push_back(name.value_or(""));
			}
		}

		/// Adds a channel to the image.
		/// 
		/// This method moves the provided channel into the image's internal storage, compressing it and adding it to the list of channels.
		/// 
		/// Example:
		/// \code{.cpp}
		/// std::span<constT> channel = ...;
		/// my_image.add_channel(channel, 1920, 1080, "red"));
		/// \endcode
		/// 
		/// \param data The channel to be added to the image.
		/// \param width The width of the channel
		/// \param height The height of the channel
		/// \param name (Optional) Channel name of the channel to be inserted. If no channel names are set this argument is ignored.
		/// \param compression_codec (Optional) Compression codec to apply to the channel, every channel is allowed to have a different one.
		/// \param compression_level (Optional) Compression level, defaults to 5.
		void add_channel(
			std::span<const T> data, 
			size_t width,
			size_t height,
			std::optional<std::string> name = std::nullopt,
			enums::codec compression_codec = enums::codec::lz4,
			size_t compression_level = 5
		)
		{
			m_Channels.push_back(compressed::channel(
				std::span<const T>(data.begin(), data.end()),
				width,
				height,
				compression_codec,
				compression_level
			));
			if (m_ChannelNames.size() > 0)
			{
				m_ChannelNames.push_back(name.value_or(""));
			}
		}

		/// \brief Prints statistical information about the image file structure.
		/// 
		/// This function outputs various details about the compressed image, 
		/// including dimensions, number of channels, compression ratio, and metadata.
		/// 
		/// Example output:
		/// 
		///		Statistics for image buffer:
		///		 Width:             1024
		///		 Height:            768
		///		 Channels:          3
		///		 Channelnames:      [R, G, B]
		///		 --------------
		///		 Compressed Size:   123456 bytes
		///		 Uncompressed Size: 3145728 bytes
		///		 Compression ratio: 25.5x
		///		 Num Chunks:        512
		///		 Metadata:
		///		 {
		///		    "author": "User",
		///		    "timestamp": "2024-03-15"
		///		 }
		void print_statistics()
		{
			size_t compressed_size = 0;
			size_t uncompressed_size = 0;
			size_t num_chunks = 0;
			for (const auto& channel : m_Channels)
			{
				compressed_size += channel.compressed_size();
				uncompressed_size += channel.uncompressed_size();
				num_chunks += channel.num_chunks();
			}

			std::cout << "Statistics for image buffer:" << std::endl;
			std::cout << " Width:             " << m_Width << std::endl;
			std::cout << " Height:            " << m_Height << std::endl;
			std::cout << " Channels:          " << m_Channels.size() << std::endl;
			std::cout << " Channelnames:      [";

			for (size_t i = 0; i < m_ChannelNames.size(); ++i)
			{
				std::cout << m_ChannelNames[i];
				if (i < m_ChannelNames.size() - 1)
				{
					std::cout << ", ";
				}
			}

			std::cout << "]" << std::endl;
			std::cout << " --------------     " << std::endl;
			std::cout << " Compressed Size:   " << compressed_size << std::endl;
			std::cout << " Uncompressed Size: " << uncompressed_size << std::endl;
			std::cout << " Compression ratio: " << static_cast<double>(uncompressed_size) / compressed_size << "x" << std::endl;
			std::cout << " Num Chunks:        " << num_chunks << std::endl;
			std::cout << " Metadata:          " << "\n " << m_Metadata.dump(4) << std::endl;
		}


		/// Return the compression ratio over all channels.
		double compression_ratio() const noexcept
		{
			size_t total_uncompressed = 1;
			size_t total_compressed = 1;
			for (const auto& channel : m_Channels)
			{
				total_compressed += channel.compressed_size();
				total_uncompressed += channel.uncompressed_size();
			}
			return static_cast<double>(total_uncompressed) / total_compressed;
		}


		// ---------------------------------------------------------------------------------------------------------------------
		// Iterators
		// ---------------------------------------------------------------------------------------------------------------------

		auto begin() noexcept { return m_Channels.begin(); }
		auto begin() const noexcept { return m_Channels.begin(); }
		auto end() noexcept { return m_Channels.end(); }
		auto end() const noexcept { return m_Channels.end(); }

		
		// ---------------------------------------------------------------------------------------------------------------------
		// Accessors
		// ---------------------------------------------------------------------------------------------------------------------

		/// Retrieves a reference to a channel by its index.
		/// 
		/// \param index The index of the channel to retrieve.
		/// \return A reference to the requested channel.
		/// \throws std::out_of_range if the index is out of bounds.
		compressed::channel<T>& channel(size_t index)
		{
			if (index >= m_Channels.size())
			{
				throw std::out_of_range("Channel index out of range");
			}
			return m_Channels[index];
		}

		/// Retrieves a reference to a channel by its name.
		/// 
		/// \param name The name of the channel to retrieve.
		/// \return A reference to the requested channel.
		/// \throws std::out_of_range if the channel name is invalid.
		compressed::channel<T>& channel(const std::string_view name)
		{
			size_t index = get_channel_offset(name);
			return m_Channels[index];
		}

		/// Retrieves references to multiple channels by name and returns them as a tuple.
		/// 
		/// Can be used with structured bindings to quickly get the specified channels from an image.
		/// These are returned as references (but don't have to be bound as such)
		/// 
		/// Example:
		/// 
		/// \code{.cpp}
		/// compressed::image my_image = ...;
		/// auto [r, g, b] = my_image.channels("r", "g", "b");
		/// \endcode
		/// 
		/// \tparam Args Variadic template arguments, each convertible to std::string.
		/// \param channel_names The names of the channels to retrieve.
		/// \return A tuple containing references to the requested channels.
		template <typename... Args>
			requires (std::conjunction_v<std::is_constructible<std::string, Args>...>)
		auto channels_ref(Args... channel_names)
		{
			return std::tie(this->channel(std::forward<Args>(channel_names))...);
		}

		/// Retrieves references to multiple channels by index and returns them as a tuple.
		/// 
		/// Can be used with structured bindings to quickly get the specified channels from an image.
		/// These are returned as references (but don't have to be bound as such)
		/// 
		/// Example:
		/// 
		/// \code{.cpp}
		/// compressed::image my_image = ...;
		/// auto [r, g, b] = my_image.channels(0, 1, 2);
		/// \endcode
		/// 
		/// \tparam Args Variadic template arguments, each convertible to size_t.
		/// \param channel_indices The indices of the channels to get
		/// \return A tuple containing references to the requested channels.
		template <typename... Args>
			requires (std::conjunction_v<std::is_convertible<size_t, Args>...>)
		auto channels(Args... channel_indices)
		{
			return std::tie(this->channel(std::forward<Args>(channel_indices))...);
		}

		/// Retrieves references to multiple channels their indices and returns them in a vector.
		/// 
		/// \param channel_indices A vector of channel indices.
		/// \return A vector containing references to the requested channels.
		/// \throws std::out_of_range if any channel indec is invalid.
		std::vector<compressed::channel<T>&> channels(std::vector<size_t> channel_indices)
		{
			std::vector<compressed::channel<T>> result{};
			for (const auto& index : channel_indices)
			{
				result.append(this->channel(index));
			}
			return result;
		}

		/// Retrieves references to multiple channels by name and returns them in a vector.
		///  
		/// \param channel_names A vector of channel names.
		/// \return A vector containing references to the requested channels.
		/// \throws std::out_of_range if any channel name is invalid.
		std::vector<compressed::channel<T>&> channels(std::vector<std::string> channel_names)
		{
			std::vector<compressed::channel<T>> result{};
			for (const auto& name : channel_names)
			{
				result.append(this->channel(name));
			}
			return result;
		}

		/// Retrieves references to all of the channels in the image
		/// 
		/// \return A vector containing references to the all the channels.
		std::vector<compressed::channel<T>>& channels()
		{
			return m_Channels;
		}

		/// Retrieves const references to all of the channels in the image
		/// 
		/// \return A vector containing references to the all the channels.
		const std::vector<compressed::channel<T>>& channels() const
		{
			return m_Channels;
		}

		/// Decompress all of the channels and return them in planar fashion.
		/// 
		/// Each channel's decompressed data is stored as a separate vector.
		/// 
		/// \return A vector of decompressed channel data, where each inner vector corresponds to a channel.
		std::vector<std::vector<T>> get_decompressed()
		{
			std::vector<std::vector<T>> result{};
			for (auto& channel : m_Channels)
			{
				result.push_back(channel.get_decompressed());
			}
			return result;
		}


		/// Retrieve the logical index of the given channel.
		/// 
		/// This function searches for the specified channel name in the list of available channels.
		/// If the channel is not found, it throws a `std::invalid_argument`.
		/// 
		/// \param channelname The name of the channel to search for.
		/// \return The index of the channel if found.
		/// \throws std::invalid_argument if the channel is not available.
		size_t get_channel_offset(const std::string_view channelname) const
		{
			for (size_t i = 0; i < m_ChannelNames.size(); ++i)
			{
				if (m_ChannelNames[i] == channelname)
				{
					return i;
				}
			}
			throw std::invalid_argument(std::format("Unknown channelname '{}' encountered", channelname));
		}

		/// Width of the Image
		size_t width() const noexcept
		{
			return m_Width;
		}

		/// Height of the image
		size_t height() const noexcept
		{
			return m_Height;
		}

		/// Total number of channels in the image
		size_t num_channels() const noexcept
		{
			return m_Channels.size();
		}

		/// Names of the channels stored on the image, are stored in the same order as the logical indices. So if the channelnames
		/// are { "B", "G", "R" } accessing channel "R" would be index 2.
		std::vector<std::string> channelnames() const noexcept
		{
			return m_ChannelNames;
		}

		/// Set the channelnames according to their logical indices, 
		void channelnames(std::vector<std::string> _channelnames) 
		{
			if (_channelnames.size() != m_Channels.size())
			{
				throw std::invalid_argument(std::format(
					"Invalid number of arguments received for setting channelnames. Expected vector size to be exactly {} but instead got {}", 
					m_Channels.size(),
					_channelnames.size()
				).c_str()
				);
			}
			m_ChannelNames = _channelnames;
		}

		/// Arbitrary user metadata, not authored or managed by image class, it's up to the caller to handle what goes in and comes out
		void metadata(const json_ordered& _metadata) noexcept
		{
			m_Metadata = _metadata;
		}

		/// Arbitrary user metadata, not authored or managed by the image class, it's up to the caller to handle what goes in and comes out
		json_ordered& metadata() noexcept
		{
			return m_Metadata;
		}

		/// Arbitrary user metadata, not authored or managed by image class, it's up to the caller to handle what goes in and comes out
		const json_ordered& metadata() const noexcept
		{
			return m_Metadata;
		}

		/// Update the number of threads used internally by c-blosc2 for compression and decompression.
		/// This is automatically set when iterating through the images with compressed::for_each for example
		/// by specifying the compression codec.
		void update_nthreads(size_t nthreads)
		{
			for (auto& channel : m_Channels)
			{
				channel.update_nthreads(nthreads);
			}
		}


	private:
		/// All the channels, each holding their own decompression and compression context.
		std::vector<compressed::channel<T>> m_Channels{};

		/// Arbitrary user metadata, not authored or managed by us, it's up to the caller to handle what goes in and comes out
		json_ordered m_Metadata{};

		/// Optional set of channelnames to associate to the channels. If not specified sensible defaults are chosen. For example,
		/// if 3 channels are provided we default to { "R", "G", "B" }
		std::vector<std::string> m_ChannelNames{};

		/// The width of the image file
		size_t m_Width = 1;

		/// The height of the image file
		size_t m_Height = 1;

	private:

#ifdef COMPRESSED_IMAGE_OIIO_AVAILABLE

		/// \brief Read a contiguous channel sequence from the passed input pointer
		///
		/// When reading with OpenImageIO it is a lot more efficient to parse as many channels as possible in one go
		/// rather than reading one channel at a time as the ImageInput keeps the data as compressed (in many cases).
		/// If we were to read one channel at a time this would significantly slow down our read speeds.
		/// 
		/// Due to us only being able to read contiguous channels at a time this helper function allows us to do that.
		/// 
		/// \param input_ptr The opened OpenImageIO ImageInput.
		/// \param chbegin The start channel to read
		/// \param chend The end channel to read
		/// \param interleaved_buffer The buffer into which we will read the channels (before then interleaving).
		///							  must be sized to exactly fit nchannels * width * height
		/// \param deinterleaved_buffer The buffers to deinterleave into, must be exactly of size nchannels with each
		///								sub-buffer being exactly width * height.
		/// \param scanlines_per_chunk The number of scanlines that fit into one chunk (exactly).
		/// \param contexts The contexts for compression, must be exactly nchannels amount
		/// \param schunks The schunks for compression, must be exactly nchannels amount
		/// \param chunk_buffer A scratch buffer for compression (from which we copy).
		/// 
		/// \throws std::invalid_argument if any of the above conditions is not met.
		static void read_contiguous_oiio_channels(
			std::unique_ptr<OIIO::ImageInput>& input_ptr,
			int chbegin,
			int chend,
			std::span<T> interleaved_buffer,
			std::vector<std::span<T>>& deinterleaved_buffer,
			size_t scanlines_per_chunk,
			std::vector<blosc2::context_ptr>& contexts,
			std::vector<blosc2::schunk<T>>& schunks,
			util::default_init_vector<std::byte>& chunk_buffer
		)
		{
			const int nchannels = chend - chbegin;
			const OIIO::ImageSpec& spec = input_ptr->spec();
			const auto typedesc = enums::get_type_desc<T>();

			// Ensure this function is called with at least 1 channel to read.
			if (nchannels < 1)
			{
				throw std::runtime_error(
					std::format(
						"read_contiguous_oiio_channels: passed number of channels is less than one. This should not happen. Got {}",
						nchannels
					)
				);
			}

			// Ensure the interleaved buffer is correctly sized.
			if (interleaved_buffer.size() != static_cast<size_t>(nchannels) * spec.width * scanlines_per_chunk)
			{
				throw std::invalid_argument(
					std::format(
						"read_contiguous_oiio_channels: Received incorrectly sized interleaved buffer, should be exactly"
						" {:L} elements large but instead got {:L}.", 
						static_cast<size_t>(nchannels) * spec.width * scanlines_per_chunk,
						interleaved_buffer.size()
					)
				);
			}
			// Ensure the deinterleaved buffer, and its subbuffers, are correctly sized.
			if (deinterleaved_buffer.size() != static_cast<size_t>(nchannels))
			{
				throw std::invalid_argument(
					std::format(
						"read_contiguous_oiio_channels: Received incorrectly sized deinterleaved buffer, should be exactly"
						" {:L} elements large but instead got {:L}.",
						nchannels,
						deinterleaved_buffer.size()
					)
				);
			}
			for (const auto& buffer : deinterleaved_buffer)
			{
				if (buffer.size() != spec.width * scanlines_per_chunk)
				{
					throw std::invalid_argument(
						std::format(
							"read_contiguous_oiio_channels: Received incorrectly sized deinterleaved buffer,"
							" should be exactly {:L} elements large but instead got {:L}.",
							static_cast<size_t>(nchannels) * spec.width * scanlines_per_chunk,
							interleaved_buffer.size()
						)
					);
				}
			}
			// Ensure the contexts and schunks are correctly sized
			if (contexts.size() != static_cast<size_t>(nchannels) || schunks.size() != static_cast<size_t>(nchannels))
			{
				throw std::runtime_error(
					std::format(
						"read_contiguous_oiio_channels: Internal error: Expected the number of passed schunks and contexts"
						" to exactly match the number of requested channels. Instead got {} and {} while {} was the expected"
						" number.",
						schunks.size(),
						contexts.size(),
						nchannels
					)
				);
			}

			// Iterate all scanlines and read as many scanlines as possible in one go, compressing them on the fly 
			// into all of the super-chunks. 
			int y = 0;
			while (y < spec.height)
			{
				_COMPRESSED_PROFILE_SCOPE("Read Scanlines and compress");
				int scanlines_to_read = static_cast<int>(std::min<size_t>(scanlines_per_chunk, static_cast<size_t>(spec.height - y)));
				input_ptr->read_scanlines(
					0, // subimage
					0, // miplevel
					y, // ybegin
					y + scanlines_to_read, // yend
					0, // z
					chbegin,
					chend,
					typedesc,
					static_cast<void*>(interleaved_buffer.data())
				);

				// Deinterleave the buffers, in some cases we may be deinterleaving empty space here but that 
				// is ok as we refit the buffers. Since in most cases the size will only be off by at most one
				// scanline. In the case of the last chunk, we may be at worst deinterleaving only one scanline
				// with the rest being empty space but that is also ok.
				image_algo::deinterleave(std::span<const T>(interleaved_buffer), deinterleaved_buffer);

				// Now start compressing the chunks and appending them into the super-chunks.
				for (auto channel_idx : std::views::iota(0, nchannels))
				{
					// How many elements we actually read per buffer
					size_t read_elements = static_cast<size_t>(scanlines_to_read) * spec.width;

					auto deinterleaved_fitted = std::span<T>(deinterleaved_buffer[channel_idx].data(), read_elements);
					schunks[channel_idx].append_chunk(
						contexts[channel_idx],
						deinterleaved_fitted,
						std::span<std::byte>(chunk_buffer)
					);
				}
				y += scanlines_to_read;
			}
		}


#endif // COMPRESSED_IMAGE_OIIO_AVAILABLE

	};

} // NAMESPACE_COMPRESSED_IMAGE