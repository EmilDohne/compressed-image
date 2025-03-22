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
#include "blosc2_wrapper.h"
#include "constants.h"
#include "channel.h"
#include "image_algo.h"

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
	template <typename T, size_t BlockSize = s_default_blocksize, size_t ChunkSize = s_default_chunksize>
	struct image : public std::ranges::view_interface<image<T, BlockSize, ChunkSize>>
	{
		static constexpr size_t block_size = BlockSize;
		static constexpr size_t chunk_size = ChunkSize;

		image() = default;
		~image() = default;
		image(const image&) = delete;					// Delete copy constructor
		image& operator=(const image&) = delete;		// Delete copy assignment
		image(image&&) noexcept = default;				// Move constructor
		image& operator=(image&&) noexcept = default;	// Move assignment


		/// Constructs an image object with the specified channels, dimensions, and optional compression parameters.
		/// 
		/// This constructor creates an image from a given set of channels
		/// The channel names can optionally be specified. The image is then compressed using the provided codec and compression level.
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
		/// \param channel_names (Optional) A list of channel names, must match the number of channels provided. If omitted or incorrect, channel names are ignored.
		/// \param compression_codec (Optional) The codec used for compression, default is `codec::lz4`.
		/// \param compression_level (Optional) The compression level, default is `5`.
		/// \throws std::runtime_error if a channel fails to be inserted.
		image(
			std::vector<std::vector<T>> channels,
			size_t width,
			size_t height,
			std::vector<std::string> channel_names = {},
			enums::codec compression_codec = enums::codec::lz4,
			size_t compression_level = 9
		)
		{
			m_Width = width;
			m_Height = height;
			m_ChannelNames = channel_names;
			auto comp_level_adjusted = util::ensure_compression_level(compression_level);

			// c-blosc2 chunks can at most be 2 gigabytes so the set chunk size should not exceed this.
			static_assert(ChunkSize < std::numeric_limits<int32_t>::max());
			static_assert(BlockSize < ChunkSize);
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
					m_Channels.push_back(compressed::channel<T, BlockSize, ChunkSize>(
						std::span<const T>(_channel.begin(), _channel.end()),
						width,
						height,
						compression_codec,
						comp_level_adjusted
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
			std::vector<channel<T, BlockSize, ChunkSize>> channels,
			size_t width,
			size_t height,
			std::vector<std::string> channel_names = {}
		)
		{
			m_Width = width;
			m_Height = height;
			m_ChannelNames = channel_names;
			// c-blosc2 chunks can at most be 2 gigabytes so the set chunk size should not exceed this.
			static_assert(ChunkSize < std::numeric_limits<int32_t>::max());
			static_assert(BlockSize < ChunkSize);

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

		/// Reads a compressed image from a file using OpenImageIO and compresses it during reading.
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
		/// \param compression_level The compression level (default: 5).
		/// \return A compressed image instance.
		static image read(
			std::filesystem::path filepath,
			enums::codec compression_codec = enums::codec::lz4,
			size_t compression_level = 9
		)
		{
			static_assert(ChunkSize % sizeof(T) == 0);
			auto comp_level_adjusted = util::ensure_compression_level(compression_level);


			// Initialize the OIIO primitives
			auto input_ptr = OIIO::ImageInput::open(filepath);
			if (!input_ptr)
			{
				throw std::invalid_argument(std::format("File {} does not exist on disk", filepath.string()));
			}
			const OIIO::ImageSpec& spec = input_ptr->spec();
			const auto typedesc = enums::get_type_desc<T>();

			if (spec.tile_width != 0)
			{
				throw std::runtime_error("Opening tiled image files is currently unsupported.");
			}

			const size_t bytes_per_scanline = static_cast<size_t>(spec.width) * spec.nchannels * sizeof(T);
			// We want each channel to be up to ChunkSize rather than having all channel
			const size_t chunk_size_all = ChunkSize * spec.nchannels;
			const size_t scanlines_per_chunk = chunk_size_all / bytes_per_scanline;

			std::vector<T> interleaved_buffer(chunk_size_all / sizeof(T));
			std::vector<std::vector<T>> deinterleaved_buffer(spec.nchannels);
			std::for_each(std::execution::par_unseq, deinterleaved_buffer.begin(), deinterleaved_buffer.end(), [](auto& buffer)
				{
					buffer.resize(ChunkSize / sizeof(T));
				});

			// Create and initialize all of our schunks and compression contexts.
			std::vector<blosc2::schunk_ptr> schunks;
			std::vector<blosc2::context_ptr> contexts;
			for ([[maybe_unused]] auto _ : std::views::iota(0, spec.nchannels))
			{
				schunks.push_back(blosc2::create_default_schunk());
				contexts.push_back(blosc2::create_compression_context<T, BlockSize>(
					schunks.back(),
					std::thread::hardware_concurrency() / 4,
					compression_codec,
					comp_level_adjusted
				));
			}

			// Buffer to hold a single chunk. We will reuse this quite frequently
			std::vector<std::vector<std::byte>> chunk_buffers(spec.nchannels);
			std::for_each(std::execution::par_unseq, chunk_buffers.begin(), chunk_buffers.end(), [](auto& buffer)
				{
					buffer = std::vector<std::byte>(blosc2::min_compressed_size<ChunkSize>());
				});

			// Iterate all scanlines and read as many scanlines as possible in one go, compressing them on the fly 
			// into all of the super-chunks. 
			int y = 0;
			while (y < spec.height)
			{
				_COMPRESSED_PROFILE_SCOPE("Read Scanlines and compress");
				int scanlines_to_read = static_cast<int>(std::min<size_t>(scanlines_per_chunk, spec.height - y));
				input_ptr->read_scanlines(
					0, // subimage
					0, // miplevel
					y, // ybegin
					y + scanlines_to_read, // yend
					0, // z
					0, // chbegin
					spec.nchannels, // chend
					typedesc,
					static_cast<void*>(interleaved_buffer.data())
				);

				// Deinterleave the buffers, in some cases we may be deinterleaving empty space here but that 
				// is ok as we refit the buffers. Since in most cases the size will only be off by at most one
				// scanline. In the case of the last chunk, we may be at worst deinterleaving only one scanline
				// with the rest being empty space but that is also ok.
				image_algo::deinterleave(std::span<const T>(interleaved_buffer.begin(), interleaved_buffer.end()), deinterleaved_buffer);

				// Now start compressing the chunks and appending them into the super-chunks.
				auto gen = std::views::iota(0, spec.nchannels);
				std::for_each(std::execution::par_unseq, gen.begin(), gen.end(), [&](size_t channel_idx)
					{
						// How many elements we actually read per buffer
						size_t read_elements = scanlines_to_read * spec.width;

						// Increment the schunks internal count. I think this could go into append_chunk? but blosc does
						// it like this.
						schunks[channel_idx]->current_nchunk = schunks[channel_idx]->nchunks;

						auto deinterleaved_fitted = std::span<T>(deinterleaved_buffer[channel_idx].data(), read_elements);
						blosc2::compress<T>(contexts[channel_idx], deinterleaved_fitted, chunk_buffers[channel_idx]);
						blosc2::append_chunk(schunks[channel_idx], chunk_buffers[channel_idx]);
					});

				y += scanlines_to_read;
			}

			// Finally create the channels from the schunk and create the image instance
			std::vector<compressed::channel<T, BlockSize, ChunkSize>> channels;
			for (const auto channel_idx : std::views::iota(0, spec.nchannels))
			{
				channels.push_back(compressed::channel<T, BlockSize, ChunkSize>(
					std::move(schunks[channel_idx]), 
					spec.width, 
					spec.height, 
					compression_codec, 
					comp_level_adjusted)
				);
			}

			return compressed::image<T, BlockSize, ChunkSize>(std::move(channels), spec.width, spec.height, spec.channelnames);
		}

#endif

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
		void add_channel(compressed::channel<T, BlockSize, ChunkSize> _channel, std::optional<std::string> name = std::nullopt)
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

		/// @brief Prints statistical information about the image file structure.
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
		compressed::channel<T, BlockSize, ChunkSize>& channel_ref(size_t index)
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
		compressed::channel<T, BlockSize, ChunkSize>& channel_ref(const std::string_view name)
		{
			size_t index = get_channel_offset(name);
			return m_Channels[index];
		}

		/// Retrieves references to multiple channels by name and returns them as a tuple.
		/// 
		/// Can be used with structured bindings to quickly get the specified channels from an image.
		/// 
		/// Example:
		/// 
		/// \code{.cpp}
		/// compressed::image my_image = ...;
		/// auto [r, g, b] = my_image.channels_ref("r", "g", "b");
		/// \endcode
		/// 
		/// \tparam Args Variadic template arguments, each convertible to std::string.
		/// \param channel_names The names of the channels to retrieve.
		/// \return A tuple containing references to the requested channels.
		template <typename... Args>
			requires (std::conjunction_v<std::is_constructible<std::string, Args>...>)
		auto channels_ref(Args... channel_names)
		{
			return std::tie(this->channel_ref(std::forward<Args>(channel_names))...);
		}

		/// Retrieves references to multiple channels by index and returns them as a tuple.
		/// 
		/// Can be used with structured bindings to quickly get the specified channels from an image.
		/// 
		/// Example:
		/// 
		/// \code{.cpp}
		/// compressed::image my_image = ...;
		/// auto [r, g, b] = my_image.channels_ref(0, 1, 2);
		/// \endcode
		/// 
		/// \tparam Args Variadic template arguments, each convertible to size_t.
		/// \param channel_indices The indices of the channels to get
		/// \return A tuple containing references to the requested channels.
		template <typename... Args>
			requires (std::conjunction_v<std::is_convertible<size_t, Args>...>)
		auto channels_ref(Args... channel_indices)
		{
			return std::tie(this->channel_ref(std::forward<Args>(channel_indices))...);
		}

		/// Retrieves references to multiple channels by name and returns them in a vector.
		/// 
		/// This method should be preferred if you don't know ahead of time which channels you want to extract.
		/// 
		/// \param channel_names A vector of channel names.
		/// \return A vector containing references to the requested channels.
		/// \throws std::out_of_range if any channel name is invalid.
		std::vector<compressed::channel<T, BlockSize, ChunkSize>&> channels_ref(const std::vector<std::string> channel_names)
		{
			std::vector<compressed::channel<T, BlockSize, ChunkSize>> result{};
			for (const auto& name : channel_names)
			{
				result.append(this->channel(name));
			}
			return result;
		}

		/// Retrieves references to all of the channels in the image
		/// 
		/// \return A vector containing references to the all the channels.
		std::vector<compressed::channel<T, BlockSize, ChunkSize>>& channels()
		{
			return m_Channels;
		}

		/// Retrieves const references to all of the channels in the image
		/// 
		/// \return A vector containing references to the all the channels.
		const std::vector<compressed::channel<T, BlockSize, ChunkSize>>& channels() const
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
		std::vector<channel<T, BlockSize, ChunkSize>> m_Channels{};

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
	};

} // NAMESPACE_COMPRESSED_IMAGE