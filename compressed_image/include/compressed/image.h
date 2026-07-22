#pragma once

#include <array>
#include <vector>
#include <span>
#include <memory>
#include <optional>
#include <limits>
#include <tuple>
#include <filesystem>
#include <chrono>

#include <blosc2.h>
#include <nlohmann/json.hpp>

#ifdef COMPRESSED_IMAGE_OIIO_AVAILABLE
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/filesystem.h>
#endif

#include "macros.h"
#include "blosc2/schunk.h"
#include "blosc2/lazyschunk.h"
#include "constants.h"
#include "channel.h"
#include "json_alias.h"
#include "image_algo.h"
#include "detail/oiio_util.h"
#include "detail/scoped_timer.h"

#include <BS_thread_pool.hpp>

namespace
NAMESPACE_COMPRESSED_IMAGE
{
    /// Compressed Image representation with easy access to different channels. Internally functions very similar to an NDArray
    /// with the important distinction that the number of dimensions is fixed to be 3-Dimensional (width, height, channels).
    /// They are laid out in scanline order with each channel being its own distinct object which may have any size.
    ///
    /// The image is stored in a non-resizable fashion so whatever the resolution was going into it, is what the image will be.
    /// To rescale or refit the image a new `image` has to be constructed.
    ///
    /// The data is compressed in-memory, and we store it as part of a blosc2 super-chunk, which is essentially a 3d array of
    /// super-chunk -> chunk -> block. Where having the block size fit into L1 cache and the Chunk size into L3 cache is desirable
    /// as each block can be handled by a single cpu core while the chunk fits well within shared L3 memory.
    template <typename T>
    struct image : public std::ranges::view_interface<image<T>>
    {
        using value_type = T;

        image() = default;
        image(image&&) = default;
        image& operator=(image&&) = default;
        image(const image&) = delete;
        image& operator=(const image&) = delete;
        ~image() = default;


        /// Constructs an image object with the specified channels, dimensions, and optional compression parameters.
        ///
        /// This constructor creates an image from a given set of channels. The channel names can optionally be specified.
        /// The image is then compressed using the provided codec and compression level.
        ///
        /// Example:
        /// \code{.cpp}
        /// std::vector<std::span<const uint8_t>> channels = ...;
        /// compressed::image<uint8_t> my_image(channels, 1920, 1080, {"r", "g", "b"}, codec::lz4, 5);
        /// \endcode
        ///
        /// \param channels A vector of spans containing the image channels (each channel is a 2D array of pixel data).
        ///					on construction these will be compressed thus the data can be safely freed after this function.
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
            std::vector<std::span<const T>> channels,
            size_t width,
            size_t height,
            const std::vector<std::string>& channel_names = {},
            enums::codec compression_codec = enums::codec::lz4,
            size_t compression_level = 9,
            size_t block_size = s_default_blocksize,
            size_t chunk_size = s_default_chunksize
        )
        {
            _COMPRESSED_PROFILE_FUNCTION();
            m_width = width;
            m_height = height;
            m_channel_names = channel_names;
            auto comp_level_adjusted = util::ensure_compression_level(compression_level);

            // c-blosc2 chunks can at most be 2 gigabytes so the set chunk size should not exceed this.
            assert(chunk_size < std::numeric_limits<int32_t>::max());
            assert(block_size < chunk_size);
            if (channel_names.size() != channels.size() && channel_names.size() != 0)
            {
                std::cout << std::format(
                    "Invalid channelnames passed to image constructor, required them to match the number of"
                    " channels in the channels parameter.Expected {} items but instead got {} names. Ignoring channel names",
                    channels.size(),
                    channel_names.size()
                ) << std::endl;

                m_channel_names = {};
            }

            // Iterate all channels and start creating channels for it.
            size_t channel_idx = 0;
            for (const auto& _channel : channels)
            {
                try
                {
                    // Generate the channel and append it.
                    m_channels.push_back(
                        NAMESPACE_COMPRESSED_IMAGE::channel<T>(
                            _channel,
                            width,
                            height,
                            compression_codec,
                            comp_level_adjusted,
                            block_size,
                            chunk_size
                        )
                    );
                }
                catch (const std::exception& e)
                {
                    if (m_channel_names.size() > 0)
                    {
                        throw std::runtime_error(
                            std::format(
                                "Failed to insert channel '{}' at position {}. Full error: \n{}",
                                m_channel_names[channel_idx],
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


        /// Constructs an image object with the specified channels, dimensions, and optional compression parameters.
        ///
        /// This constructor creates an image from a given set of channels. The channel names can optionally be specified.
        /// The image is then compressed using the provided codec and compression level.
        ///
        /// Example:
        /// \code{.cpp}
        /// std::vector<std::vector<uint8_t>> channels = ...;
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
            _COMPRESSED_PROFILE_FUNCTION();
            m_width = width;
            m_height = height;
            m_channel_names = channel_names;
            auto comp_level_adjusted = util::ensure_compression_level(compression_level);

            // c-blosc2 chunks can at most be 2 gigabytes so the set chunk size should not exceed this.
            assert(chunk_size < std::numeric_limits<int32_t>::max());
            assert(block_size < chunk_size);
            if (channel_names.size() != channels.size() && channel_names.size() != 0)
            {
                std::cout << std::format(
                    "Invalid channelnames passed to image constructor, required them to match the number of"
                    " channels in the channels parameter.Expected {} items but instead got {} names. Ignoring channel names",
                    channels.size(),
                    channel_names.size()
                ) << std::endl;

                m_channel_names = {};
            }

            // Iterate all channels and start creating channels for it.
            size_t channel_idx = 0;
            for (const auto& _channel : channels)
            {
                try
                {
                    // Generate the channel and append it.
                    m_channels.push_back(
                        NAMESPACE_COMPRESSED_IMAGE::channel<T>(
                            std::span<const T>(_channel.begin(), _channel.end()),
                            width,
                            height,
                            compression_codec,
                            comp_level_adjusted,
                            block_size,
                            chunk_size
                        )
                    );
                }
                catch (const std::exception& e)
                {
                    if (m_channel_names.size() > 0)
                    {
                        throw std::runtime_error(
                            std::format(
                                "Failed to insert channel '{}' at position {}. Full error: \n{}",
                                m_channel_names[channel_idx],
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


        /// Constructs an image object with the specified channels and dimensions, optionally passing channelnames.
        ///
        /// This constructor creates an image from a given set of channels. The channel names can optionally be specified.
        /// The passed channels should already be compressed::channel instances.
        ///
        ///
        /// \param channels A vector of compressed::channel instances to initialize the image with.
        /// \param width The width of the image in pixels.
        /// \param height The height of the image in pixels.
        /// \param channel_names (Optional) A list of channel names, must match the number of channels provided.
        ///					     If omitted or incorrect, channel names are ignored.
        image(
            std::vector<NAMESPACE_COMPRESSED_IMAGE::channel<T>> channels,
            size_t width,
            size_t height,
            std::vector<std::string> channel_names = {}
        )
        {
            _COMPRESSED_PROFILE_FUNCTION();
            m_width = width;
            m_height = height;
            m_channel_names = channel_names;

            if (channel_names.size() != channels.size() && channel_names.size() != 0)
            {
                std::cout << std::format(
                    "Invalid channelnames passed to image constructor, required them to match the number of"
                    " channels in the channels parameter.Expected {} items but instead got {} names. Ignoring channel names",
                    channels.size(),
                    channel_names.size()
                ) << std::endl;

                m_channel_names = {};
            }

            size_t counter = 0;
            for (auto& channel : channels)
            {
                if (channel.width() != width)
                {
                    throw std::invalid_argument(
                        std::format(
                            "Invalid channel passed to compressed::image constructor at index {}. It's width does not"
                            " equal {} but instead is {}",
                            counter,
                            width,
                            channel.width()
                        )
                    );
                }
                if (channel.height() != height)
                {
                    throw std::invalid_argument(
                        std::format(
                            "Invalid channel passed to compressed::image constructor at index {}. It's height does not"
                            " equal {} but instead is {}",
                            counter,
                            height,
                            channel.height()
                        )
                    );
                }

                ++counter;
            }
            m_channels = std::move(channels);
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
        /// auto img = compressed::image::read<uint8_t>(filepath, 0, compressed::enums::codec::lz4, 5);
        /// \endcode
        ///
        /// \param filepath The file path of the image to read.
        /// \param subimage The subimage to extract the channels from (default: 0). Only relevant for multi-part images.
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
            int subimage = 0,
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

            // Ensure we seek to the right subimage before retrieving the spec as it is subimage dependent.
            auto res = input_ptr->seek_subimage(subimage, 0);
            if (!res)
            {
                throw std::invalid_argument(
                    std::format(
                        "File '{}' does not have a subimage {}, cannot seek to it",
                        filepath.string(),
                        subimage
                    )
                );
            }
            const OIIO::ImageSpec& spec = input_ptr->spec();

            return image<T>::read(
                std::move(input_ptr),
                spec.channelnames,
                subimage,
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
        /// This overload allows you to specify a custom invocable function which is executed after a chunk has been read
        /// and before it is compressed. If you have some common operations like color management or a filter which you
        /// wish to apply this would go in here.
        /// Specifying these right away in the read is much more efficient than iterating over the image again later and
        /// applying these.
        ///
        /// The function passed should have no notion of coordinates or similar, it should simply assume to receive a block
        /// of data (that is part of an image) as well as the channel index we are currently operating over.
        ///
        /// Example:
        /// \code{.cpp}
        /// std::filesystem::path filepath = "image.exr";
        ///
        /// // Read an image file and apply a post-process which adds 1 to the pixel value for all RGB channels (0, 1, 2).
        ///
        /// auto postprocess = [](size_t channel_idx, std::span<T> chunk)
        ///		{
        ///			if (channel_idx > 2)
        ///			{
        ///				return;
        ///			}
        ///
        ///			std::for_each(std::execution::par_unseq, chunk.begin(), chunk.end(), [](T& value)
        ///			{
        ///				value += 1;
        ///			}
        ///		};
        ///
        /// auto img = compressed::image::read<uint8_t>(
        ///		filepath,
        ///		std::forward(postprocess),
        ///		0, // subimage
        ///		compressed::enums::codec::lz4, // compression_code
        ///		5 // compression_level
        /// );
        /// \endcode
        ///
        /// \param filepath The file path of the image to read.
        /// \param postprocess A postprocessing function to run after read but before re-compression. This function should
        ///					   take a `size_t` and a `std::span<T>` where the `size_t` is the channel index we are currently
        ///					   iterating over (e.g. 3 for the alpha channel) and the `std::span<T>` is a chunk within that
        ///					   channel, where this chunk is and what coordinates it represents is not passed along.
        /// \param subimage The subimage to extract the channels from (default: 0). Only relevant for multi-part images.
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
        template <typename PostProcess>
            requires std::invocable<std::remove_reference_t<PostProcess>, size_t, std::span<T>>
        static image read(
            std::filesystem::path filepath,
            PostProcess&& postprocess,
            int subimage = 0,
            enums::codec compression_codec = enums::codec::lz4,
            size_t compression_level = 9,
            size_t block_size = s_default_blocksize,
            size_t chunk_size = s_default_chunksize
        )
        {
            _COMPRESSED_PROFILE_FUNCTION();
            // Initialize the OIIO primitives
            auto input_ptr = OIIO::ImageInput::open(filepath);
            if (!input_ptr)
            {
                throw std::invalid_argument(std::format("File {} does not exist on disk", filepath.string()));
            }

            // Ensure we seek to the right subimage before retrieving the spec as it is subimage dependent.
            auto res = input_ptr->seek_subimage(subimage, 0);
            if (!res)
            {
                throw std::invalid_argument(
                    std::format(
                        "File '{}' does not have a subimage {}, cannot seek to it",
                        filepath.string(),
                        subimage
                    )
                );
            }
            const OIIO::ImageSpec& spec = input_ptr->spec();

            return image<T>::read(
                std::move(input_ptr),
                std::forward<PostProcess>(postprocess),
                spec.channelnames,
                subimage,
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
        /// \param input_ptr The opened OIIO input pointer.
        /// \param channelindices The channels you wish to extract. These may be specified in any order. We throw a
        ///						  std::out_of_range if one of the passed channels does not exist. It is perfectly valid
        ///						  to e.g. call this with {3, 1, 2} when the underlying channel structure may be
        ///						  RGBA. Sorting these back into their underlying channel structure is done on read.
        /// \param subimage The subimage to extract the channels from (default: 0). Only relevant for multi-part images.
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
            int subimage = 0,
            enums::codec compression_codec = enums::codec::lz4,
            size_t compression_level = 9,
            size_t block_size = s_default_blocksize,
            size_t chunk_size = s_default_chunksize
        )
        {
            _COMPRESSED_PROFILE_FUNCTION();
            std::vector<std::string> channelnames{};

            // Ensure we seek to the right subimage before retrieving the spec as it is subimage dependent.
            auto res = input_ptr->seek_subimage(subimage, 0);
            if (!res)
            {
                throw std::invalid_argument(
                    std::format(
                        "File does not have a subimage {}, cannot seek to it",
                        subimage
                    )
                );
            }
            const auto& spec = input_ptr->spec();

            for (int i : channelindices)
            {
                channelnames.push_back(spec.channelnames.at(i));
            }

            return image<T>::read(
                std::move(input_ptr),
                std::move(channelnames),
                subimage,
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
        /// This function allows you to specify a custom invocable function which is executed after a chunk has been read
        /// and before it is compressed. If you have some common operations like color management or a filter which you
        /// wish to apply this would go in here.
        /// Specifying these right away in the read is much more efficient than iterating over the image again later and
        /// applying these.
        ///
        /// The function passed should have no notion of coordinates or similar, it should simply assume to receive a block
        /// of data (that is part of an image) as well as the channel index we are currently operating over.
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
        /// auto postprocess = [](size_t channel_idx, std::span<T> chunk)
        ///		{
        ///			if (channel_idx > 2)
        ///			{
        ///				return;
        ///			}
        ///
        ///			std::for_each(std::execution::par_unseq, chunk.begin(), chunk.end(), [](T& value)
        ///			{
        ///				value += 1;
        ///			}
        ///		};
        ///
        /// // Read an image file and apply a post-process which adds 1 to the pixel value for all RGB channels (0, 1, 2).
        /// auto img = compressed::image::read<uint8_t>(
        ///		std::move(input_ptr),
        ///		std::forward(postprocess),
        ///		{ 0, 1, 2, 3}, // only read the RGBA channels
        ///		0, // subimage
        ///		compressed::enums::codec::lz4,
        ///		5
        /// );
        /// \endcode
        ///
        /// \param input_ptr The opened OIIO input pointer.
        /// \param postprocess A postprocessing function to run after read but before re-compression. This function should
        ///					   take a `size_t` and a `std::span<T>` where the `size_t` is the channel index we are currently
        ///					   iterating over (e.g. 3 for the alpha channel) and the `std::span<T>` is a chunk within that
        ///					   channel, where this chunk is and what coordinates it represents is not passed along.
        /// \param channelindices The channels you wish to extract. These may be specified in any order. We throw a
        ///						  std::out_of_range if one of the passed channels does not exist. It is perfectly valid
        ///						  to e.g. call this with {3, 1, 2} when the underlying channel structure may be
        ///						  RGBA. Sorting these back into their underlying channel structure is done on read.
        /// \param subimage The subimage to extract the channels from (default: 0). Only relevant for multi-part images.
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
        template <typename PostProcess>
            requires std::invocable<std::remove_reference_t<PostProcess>, size_t, std::span<T>>
        static image read(
            std::unique_ptr<OIIO::ImageInput> input_ptr,
            PostProcess&& postprocess,
            std::vector<int> channelindices,
            int subimage = 0,
            enums::codec compression_codec = enums::codec::lz4,
            size_t compression_level = 9,
            size_t block_size = s_default_blocksize,
            size_t chunk_size = s_default_chunksize
        )
        {
            _COMPRESSED_PROFILE_FUNCTION();
            std::vector<std::string> channelnames{};

            // Ensure we seek to the right subimage before retrieving the spec as it is subimage dependent.
            auto res = input_ptr->seek_subimage(subimage, 0);
            if (!res)
            {
                throw std::invalid_argument(
                    std::format(
                        "File does not have a subimage {}, cannot seek to it",
                        subimage
                    )
                );
            }
            const auto& spec = input_ptr->spec();

            for (int i : channelindices)
            {
                channelnames.push_back(spec.channelnames.at(i));
            }

            return image<T>::read(
                std::move(input_ptr),
                std::forward<PostProcess>(postprocess),
                subimage,
                std::move(channelnames),
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
        /// auto img = compressed::image::read<uint8_t>(std::move(input_ptr), {"R", "G", "B", "A"});
        /// \endcode
        ///
        /// \param input_ptr The opened OIIO input pointer.
        /// \param channelnames The channels you wish to extract. These may be specified in any order. We throw a
        ///						std::out_of_range if one of the passed channels does not exist. It is perfectly valid
        ///						to e.g. call this with {"G", "R", "A"} when the underlying channel structure may be
        ///						RGBA. Sorting these back into their underlying channel structure is done on read.
        /// \param subimage The subimage to extract the channels from (default: 0). Only relevant for multi-part images.
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
            int subimage = 0,
            enums::codec compression_codec = enums::codec::lz4,
            size_t compression_level = 9,
            size_t block_size = s_default_blocksize,
            size_t chunk_size = s_default_chunksize
        )
        {
            _COMPRESSED_PROFILE_FUNCTION();
            return image<T>::read_impl(
                std::move(input_ptr),
                std::move(channelnames),
                std::nullopt,
                subimage,
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
        /// This function allows you to specify a custom invocable function which is executed after a chunk has been read
        /// and before it is compressed. If you have some common operations like color management or a filter which you
        /// wish to apply this would go in here.
        /// Specifying these right away in the read is much more efficient than iterating over the image again later and
        /// applying these.
        ///
        /// The function passed should have no notion of coordinates or similar, it should simply assume to receive a block
        /// of data (that is part of an image) as well as the channel index we are currently operating over.
        ///
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
        /// auto postprocess = [](size_t channel_idx, std::span<T> chunk)
        ///		{
        ///			if (channel_idx > 2)
        ///			{
        ///				return;
        ///			}
        ///
        ///			std::for_each(std::execution::par_unseq, chunk.begin(), chunk.end(), [](T& value)
        ///			{
        ///				value += 1;
        ///			}
        ///		};
        ///
        /// // Read an image file and apply a post-process which adds 1 to the pixel value for all RGB channels (0, 1, 2).
        /// auto img = compressed::image::read<uint8_t>(
        ///		std::move(input_ptr),
        ///		std::forward(postprocess),
        ///		{ 0, 1, 2, 3}, // only read the RGBA channels
        ///		0, // subimage
        ///		compressed::enums::codec::lz4,
        ///		5
        /// );
        /// \endcode
        ///
        /// \param input_ptr The opened OIIO input pointer.
        /// \param postprocess A postprocessing function to run after read but before re-compression. This function should
        ///					   take a `size_t` and a `std::span<T>` where the `size_t` is the channel index we are currently
        ///					   iterating over (e.g. 3 for the alpha channel) and the `std::span<T>` is a chunk within that
        ///					   channel, where this chunk is and what coordinates it represents is not passed along.
        /// \param channelnames The channels you wish to extract. These may be specified in any order. We throw a
        ///						std::out_of_range if one of the passed channels does not exist. It is perfectly valid
        ///						to e.g. call this with {"G", "R", "A"} when the underlying channel structure may be
        ///						RGBA. Sorting these back into their underlying channel structure is done on read.
        /// \param subimage The subimage to extract the channels from (default: 0). Only relevant for multi-part images.
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
        template <typename PostProcess>
            requires std::invocable<std::remove_reference_t<PostProcess>, size_t, std::span<T>>
        static image read(
            std::unique_ptr<OIIO::ImageInput> input_ptr,
            PostProcess&& postprocess,
            std::vector<std::string> channelnames,
            int subimage = 0,
            enums::codec compression_codec = enums::codec::lz4,
            size_t compression_level = 9,
            size_t block_size = s_default_blocksize,
            size_t chunk_size = s_default_chunksize
        )
        {
            _COMPRESSED_PROFILE_FUNCTION();
            return image<T>::read_impl(
                std::move(input_ptr),
                std::move(channelnames),
                std::forward<PostProcess>(postprocess),
                subimage,
                compression_codec,
                compression_level,
                block_size,
                chunk_size
            );
        }


        /// \brief Read the metadata from the openimageio pointer into a json representation
        /// \return The metadata encoded as json. This does not recursively parse jsons!
        static json_ordered read_oiio_metadata(const OIIO::ImageSpec& spec)
        {
            return detail::param_value::to_json(spec.extra_attribs);
        }

        /// \brief Read the metadata from the file into a json representation
        ///
        /// \throws std::invalid_argument if the file does not exist on disk.
        ///
        /// \return The metadata encoded as json. This does not recursively parse jsons!
        static json_ordered read_oiio_metadata(std::filesystem::path filepath)
        {
            // Initialize the OIIO primitives
            auto input_ptr = OIIO::ImageInput::open(filepath);
            if (!input_ptr)
            {
                throw std::invalid_argument(std::format("File {} does not exist on disk", filepath.string()));
            }

            return detail::param_value::to_json(input_ptr->spec().extra_attribs);
        }

        /// \brief Write the image to disk via OpenImageIO.
        ///
        /// The output format is inferred from the file extension (e.g. `.exr`, `.tif`, `.png`).
        ///
        /// The image is written scanline-by-scanline: for each chunk index the corresponding chunk of
        /// every channel is decompressed, interleaved and flushed as complete scanlines. Peak extra
        /// memory is therefore roughly one chunk per channel rather than the whole uncompressed image,
        /// mirroring the streaming behaviour of the `read()` methods.
        ///
        /// Channel names (if set) and simple scalar metadata (string / integer / float) are written to
        /// the file; array-valued metadata is currently not written back
        /// (see \ref detail::param_value::from_json).
        ///
        /// \param filepath The destination path; its extension selects the output format.
        /// \throws std::runtime_error if the image has no channels, the dtype is unsupported by
        ///         OpenImageIO, or the file cannot be created/written.
        void write(const std::filesystem::path& filepath) const
        {
            _COMPRESSED_PROFILE_FUNCTION();

            if (m_channels.empty())
            {
                throw std::runtime_error(
                    std::format("Unable to write image to '{}': the image has no channels.", filepath.string()));
            }

            const OIIO::TypeDesc type = enums::get_type_desc<T>();
            if (type == OIIO::TypeDesc::UNKNOWN)
            {
                throw std::runtime_error(
                    std::format("Unable to write image to '{}': the element type is not supported by OpenImageIO.",
                        filepath.string()));
            }

            // Validate that all channels share one chunk layout (throws with a clear message if not);
            // this guarantees chunk index `ci` covers the same pixel range in every channel below.
            (void)this->chunk_size();

            auto out = OIIO::ImageOutput::create(filepath.string());
            if (!out)
            {
                throw std::runtime_error(
                    std::format("Unable to create an image writer for '{}': {}",
                        filepath.string(), OIIO::geterror()));
            }

            const int width = static_cast<int>(this->width());
            const int height = static_cast<int>(this->height());
            const int nchannels = static_cast<int>(this->num_channels());

            OIIO::ImageSpec spec(width, height, nchannels, type);
            if (!m_channel_names.empty())
            {
                spec.channelnames = m_channel_names;
            }
            detail::param_value::from_json(spec, m_metadata);

            if (!out->open(filepath.string(), spec))
            {
                throw std::runtime_error(
                    std::format("Unable to open '{}' for writing: {}", filepath.string(), out->geterror()));
            }

            // Stream chunk-by-chunk. `band` accumulates interleaved pixels that have not yet formed a
            // whole number of scanlines (a chunk boundary need not fall on a scanline boundary), and we
            // flush every complete scanline as soon as it is available.
            const size_t num_chunks = m_channels.front().num_chunks();
            const size_t row_stride = static_cast<size_t>(width) * static_cast<size_t>(nchannels);

            std::vector<T> band;
            std::vector<std::vector<T>> per_channel(static_cast<size_t>(nchannels));
            int next_y = 0;

            for (size_t ci = 0; ci < num_chunks; ++ci)
            {
                const size_t elems = m_channels.front().chunk_elems(ci);
                for (int c = 0; c < nchannels; ++c)
                {
                    per_channel[static_cast<size_t>(c)].resize(elems);
                    m_channels[static_cast<size_t>(c)].get_chunk(std::span<T>(per_channel[static_cast<size_t>(c)]), ci);
                }

                const size_t base = band.size();
                band.resize(base + elems * static_cast<size_t>(nchannels));
                for (size_t p = 0; p < elems; ++p)
                {
                    for (int c = 0; c < nchannels; ++c)
                    {
                        band[base + p * static_cast<size_t>(nchannels) + static_cast<size_t>(c)] =
                            per_channel[static_cast<size_t>(c)][p];
                    }
                }

                const size_t complete_rows = band.size() / row_stride;
                if (complete_rows > 0)
                {
                    const int y_end = next_y + static_cast<int>(complete_rows);
                    if (!out->write_scanlines(next_y, y_end, 0, type, band.data()))
                    {
                        throw std::runtime_error(
                            std::format("Failed to write scanlines to '{}': {}", filepath.string(), out->geterror()));
                    }
                    next_y = y_end;
                    band.erase(band.begin(), band.begin() + complete_rows * row_stride);
                }
            }

            if (!out->close())
            {
                throw std::runtime_error(
                    std::format("Failed to finalize '{}': {}", filepath.string(), out->geterror()));
            }
        }

        /// \brief Read a compressed image from an in-memory encoded image buffer via OpenImageIO.
        ///
        /// This is the in-memory counterpart to \ref read: instead of a path on disk it takes a buffer
        /// containing a full encoded image (e.g. the bytes of an `.exr`/`.png` file) and an OpenImageIO
        /// IOProxy is used to decode it without touching the filesystem. See GitHub issue #7.
        ///
        /// \param buffer   The encoded image bytes.
        /// \param format   Format/extension hint used to select the OpenImageIO reader, e.g. "exr", "png".
        /// \param channelnames Channels to read; empty reads every channel of the subimage.
        /// \param subimage The subimage to read.
        /// \throws std::runtime_error / std::invalid_argument if the buffer cannot be decoded.
        static image read_from_memory(
            std::span<const std::byte> buffer,
            const std::string& format,
            std::vector<std::string> channelnames = {},
            int subimage = 0,
            enums::codec compression_codec = enums::codec::lz4,
            size_t compression_level = 9,
            size_t block_size = s_default_blocksize,
            size_t chunk_size = s_default_chunksize
        )
        {
            _COMPRESSED_PROFILE_FUNCTION();

            // The IOMemReader must outlive every use of the ImageInput, so it lives in this scope and
            // the delegated read() (which consumes the input fully) completes before we return.
            // IOMemReader only reads from the buffer; the const_cast keeps this compatible with
            // OpenImageIO versions whose constructor takes a non-const void*.
            OIIO::Filesystem::IOMemReader memreader(
                const_cast<void*>(static_cast<const void*>(buffer.data())), buffer.size());
            OIIO::Filesystem::IOProxy* proxy = &memreader;

            OIIO::ImageSpec config;
            config.attribute("oiio:ioproxy", OIIO::TypeDesc::PTR, &proxy);

            const std::string name_hint = "buffer." + format;
            auto input_ptr = OIIO::ImageInput::open(name_hint, &config);
            if (!input_ptr)
            {
                throw std::runtime_error(
                    std::format("Unable to decode in-memory image (format hint '{}'): {}",
                        format, OIIO::geterror()));
            }

            if (channelnames.empty())
            {
                if (!input_ptr->seek_subimage(subimage, 0))
                {
                    throw std::invalid_argument(
                        std::format("In-memory image has no subimage {}", subimage));
                }
                channelnames = input_ptr->spec().channelnames;
            }

            return image<T>::read(
                std::move(input_ptr),
                std::move(channelnames),
                subimage,
                compression_codec,
                compression_level,
                block_size,
                chunk_size
            );
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
        void add_channel(NAMESPACE_COMPRESSED_IMAGE::channel<T> _channel,
                         std::optional<std::string> name = std::nullopt)
        {
            if (_channel.width() != this->width())
            {
                throw std::invalid_argument(
                    std::format(
                        "Cannot add channel '{}' to the image as its width does not match that of the image."
                        " Expected {:L} pixels but instead got {:L} pixels",
                        name.value_or(""),
                        this->width(),
                        _channel.width()
                    )
                );
            }
            if (_channel.height() != this->height())
            {
                throw std::invalid_argument(
                    std::format(
                        "Cannot add channel '{}' to the image as its height does not match that of the image."
                        " Expected {:L} pixels but instead got {:L} pixels",
                        name.value_or(""),
                        this->height(),
                        _channel.height()
                    )
                );
            }

            if (name.has_value() && m_channel_names.size() == m_channels.size())
            {
                m_channel_names.push_back(name.value());
            }
            else if (m_channel_names.size() > 0)
            {
                m_channel_names.push_back(name.value_or(""));
            }

            m_channels.push_back(std::move(_channel));
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
            uint8_t compression_level = 5
        )
        {
            if (width != this->width())
            {
                throw std::invalid_argument(
                    std::format(
                        "Cannot add channel '{}' to the image as its width does not match that of the image."
                        " Expected {:L} pixels but instead got {:L} pixels",
                        name.value_or(""),
                        width,
                        this->width()
                    )
                );
            }
            if (height != this->height())
            {
                throw std::invalid_argument(
                    std::format(
                        "Cannot add channel '{}' to the image as its height does not match that of the image."
                        " Expected {:L} pixels but instead got {:L} pixels",
                        name.value_or(""),
                        height,
                        this->height()
                    )
                );
            }

            if (name.has_value() && m_channel_names.size() == m_channels.size())
            {
                m_channel_names.push_back(name.value());
            }
            else if (m_channel_names.size() > 0)
            {
                m_channel_names.push_back(name.value_or(""));
            }

            m_channels.push_back(
                NAMESPACE_COMPRESSED_IMAGE::channel(
                    std::span<const T>(data.begin(), data.end()),
                    width,
                    height,
                    compression_codec,
                    compression_level
                )
            );
        }


        /// Remove a channel by its index.
        ///
        /// \param index The index of the channel to remove.
        /// \throws std::out_of_range if the index is out of bounds.
        void remove_channel(size_t index)
        {
            // Extract the channel and let it exit the scope to destruct
            auto channel = this->extract_channel(index);
        }

        /// Remove a channel by its name.
        ///
        /// \param name The name of the channel to remove.
        /// \throws std::out_of_range if the channel name is invalid.
        void remove_channel(const std::string_view name)
        {
            // Extract the channel and let it exit the scope to destruct
            auto channel = this->extract_channel(name);
        }

        /// Extracts a channel by its index.
        ///
        /// Remove the channel from the image and gives you full control over the channel. Also erases
        /// its channel name.
        ///
        /// \param index The index of the channel to retrieve.
        /// \return The channel object.
        /// \throws std::out_of_range if the index is out of bounds.
        NAMESPACE_COMPRESSED_IMAGE::channel<T> extract_channel(size_t index)
        {
            if (index >= m_channels.size())
            {
                throw std::out_of_range("Channel index out of range");
            }
            auto ret = std::move(m_channels[index]);

            m_channels.erase(m_channels.begin() + index);
            m_channel_names.erase(m_channel_names.begin() + index);

            return std::move(ret);
        }

        /// Extracts a channel by its name.
        ///
        /// Remove the channel from the image and gives you full control over the channel. Also erases
        /// its channel name.
        ///
        /// \param name The name of the channel to retrieve.
        /// \return The channel object.
        /// \throws std::out_of_range if the channel name is invalid.
        NAMESPACE_COMPRESSED_IMAGE::channel<T> extract_channel(const std::string_view name)
        {
            size_t index = get_channel_offset(name);
            return extract_channel(index);
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
            for (const auto& channel : m_channels)
            {
                compressed_size += channel.compressed_bytes();
                uncompressed_size += channel.uncompressed_size();
                num_chunks += channel.num_chunks();
            }

            std::cout << "Statistics for image buffer:" << std::endl;
            std::cout << " Width:             " << m_width << std::endl;
            std::cout << " Height:            " << m_height << std::endl;
            std::cout << " Channels:          " << m_channels.size() << std::endl;
            std::cout << " Channelnames:      [";

            for (size_t i = 0; i < m_channel_names.size(); ++i)
            {
                std::cout << m_channel_names[i];
                if (i < m_channel_names.size() - 1)
                {
                    std::cout << ", ";
                }
            }

            std::cout << "]" << std::endl;
            std::cout << " --------------     " << std::endl;
            std::cout << " Compressed Size:   " << compressed_size << std::endl;
            std::cout << " Uncompressed Size: " << uncompressed_size << std::endl;
            std::cout << " Compression ratio: " << static_cast<double>(uncompressed_size) / compressed_size << "x" <<
                std::endl;
            std::cout << " Num Chunks:        " << num_chunks << std::endl;
            std::cout << " Metadata:          " << "\n " << m_metadata.dump(4) << std::endl;
        }


        size_t compressed_bytes() const
        {
            size_t total_compressed = 0;
            for (const auto& channel : m_channels)
            {
                total_compressed += channel.compressed_bytes();
            }
            return total_compressed;
        }

        size_t uncompressed_bytes() const
        {
            size_t total_uncompressed = 0;
            for (const auto& channel : m_channels)
            {
                total_uncompressed += channel.uncompressed_size() * sizeof(T);
            }
            return total_uncompressed;
        }

        /// Return the compression ratio over all channels.
        double compression_ratio() const noexcept
        {
            const size_t compressed_bytes = std::max(size_t{1}, this->compressed_bytes());
            const size_t uncompressed_bytes = std::max(size_t{1}, this->uncompressed_bytes());
            return static_cast<double>(uncompressed_bytes) / compressed_bytes;
        }


        // ---------------------------------------------------------------------------------------------------------------------
        // Iterators
        // ---------------------------------------------------------------------------------------------------------------------

        auto begin() noexcept { return m_channels.begin(); }
        auto begin() const noexcept { return m_channels.begin(); }
        auto end() noexcept { return m_channels.end(); }
        auto end() const noexcept { return m_channels.end(); }


        // ---------------------------------------------------------------------------------------------------------------------
        // Accessors
        // ---------------------------------------------------------------------------------------------------------------------

        /// Retrieves a reference to a channel by its index.
        ///
        /// \param index The index of the channel to retrieve.
        /// \return A reference to the requested channel.
        /// \throws std::out_of_range if the index is out of bounds.
        NAMESPACE_COMPRESSED_IMAGE::channel<T>& channel(size_t index)
        {
            if (index >= m_channels.size())
            {
                throw std::out_of_range("Channel index out of range");
            }
            return m_channels[index];
        }

        /// Retrieves a reference to a channel by its name.
        ///
        /// \param name The name of the channel to retrieve.
        /// \return A reference to the requested channel.
        /// \throws std::out_of_range if the channel name is invalid.
        NAMESPACE_COMPRESSED_IMAGE::channel<T>& channel(const std::string_view name)
        {
            size_t index = get_channel_offset(name);
            return m_channels[index];
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
        auto channels(Args... channel_names)
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
        std::vector<NAMESPACE_COMPRESSED_IMAGE::channel<T>&> channels(std::vector<size_t> channel_indices)
        {
            std::vector<NAMESPACE_COMPRESSED_IMAGE::channel<T>> result{};
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
        std::vector<NAMESPACE_COMPRESSED_IMAGE::channel<T>&> channels(std::vector<std::string> channel_names)
        {
            std::vector<NAMESPACE_COMPRESSED_IMAGE::channel<T>> result{};
            for (const auto& name : channel_names)
            {
                result.append(this->channel(name));
            }
            return result;
        }

        /// Retrieves references to all of the channels in the image
        ///
        /// \return A vector containing references to the all the channels.
        std::vector<NAMESPACE_COMPRESSED_IMAGE::channel<T>>& channels()
        {
            return m_channels;
        }

        /// Retrieves const references to all of the channels in the image
        ///
        /// \return A vector containing references to the all the channels.
        const std::vector<NAMESPACE_COMPRESSED_IMAGE::channel<T>>& channels() const
        {
            return m_channels;
        }

        /// Decompress all of the channels and return them in planar fashion.
        ///
        /// Each channel's decompressed data is stored as a separate vector.
        ///
        /// \return A vector of decompressed channel data, where each inner vector corresponds to a channel.
        std::vector<std::vector<T>> get_decompressed() const
        {
            std::vector<std::vector<T>> result{};
            for (const auto& channel : m_channels)
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
            for (size_t i = 0; i < m_channel_names.size(); ++i)
            {
                if (m_channel_names[i] == channelname)
                {
                    return i;
                }
            }
            throw std::invalid_argument(std::format("Unknown channelname '{}' encountered", channelname));
        }

        /// Width of the Image
        size_t width() const noexcept
        {
            return m_width;
        }

        /// Height of the image
        size_t height() const noexcept
        {
            return m_height;
        }

        /// Total number of channels in the image
        size_t num_channels() const noexcept
        {
            return m_channels.size();
        }

        /// Names of the channels stored on the image, are stored in the same order as the logical indices. So if the channelnames
        /// are { "B", "G", "R" } accessing channel "R" would be index 2.
        std::vector<std::string> channelnames() const noexcept
        {
            return m_channel_names;
        }

        /// Set the channelnames according to their logical indices,
        void channelnames(std::vector<std::string> _channelnames)
        {
            if (_channelnames.size() != m_channels.size())
            {
                throw std::invalid_argument(
                    std::format(
                        "Invalid number of arguments received for setting channelnames. Expected vector size to be exactly {} but instead got {}",
                        m_channels.size(),
                        _channelnames.size()
                    ).c_str()
                );
            }
            m_channel_names = _channelnames;
        }

        /// Arbitrary user metadata, not authored or managed by image class, it's up to the caller to handle what goes in and comes out
        void metadata(const json_ordered& _metadata) noexcept
        {
            m_metadata = _metadata;
        }

        /// Arbitrary user metadata, not authored or managed by the image class, it's up to the caller to handle what goes in and comes out
        json_ordered& metadata() noexcept
        {
            return m_metadata;
        }

        /// Arbitrary user metadata, not authored or managed by image class, it's up to the caller to handle what goes in and comes out
        const json_ordered& metadata() const noexcept
        {
            return m_metadata;
        }

        /// Update the number of threads used internally by c-blosc2 for compression and decompression.
        /// This is automatically set when iterating through the images with compressed::for_each for example
        /// by specifying the compression codec.
        void update_nthreads(size_t nthreads)
        {
            for (auto& channel : m_channels)
            {
                channel.update_nthreads(nthreads);
            }
        }

        /// \brief Get the chunk size used for compression, this is the same across all channels.
        ///
        /// \throws std::runtime_error If the channels of the image do not all share the same chunk size as this is
        ///							   currently unsupported.
        ///
        /// \return The chunk size in bytes.
        size_t chunk_size() const
        {
            size_t chunk_size = 0;
            for (const auto& channel : m_channels)
            {
                if (chunk_size != 0 && channel.chunk_size() != chunk_size)
                {
                    throw std::runtime_error(
                        "Validation Error: Channels in image do not all have the same chunk size. This is currently"
                        " unsupported."
                    );
                }
                chunk_size = channel.chunk_size();
            }
            return chunk_size;
        }

        size_t block_size() const
        {
            size_t block_size = 0;
            for (const auto& channel : m_channels)
            {
                if (block_size != 0 && channel.block_size() != block_size)
                {
                    throw std::runtime_error(
                        "Validation Error: Channels in image do not all have the same block size. This is currently"
                        " unsupported."
                    );
                }
                block_size = channel.block_size();
            }
            return block_size;
        }

    private:
        struct ring_buffer_slot
        {
            util::default_init_vector<T> interleaved_buffer;
            std::vector<util::default_init_vector<T>> deinterleaved_buffer;
            std::vector<cuda::scoped_host_pinner> memory_pinners;
            std::future<void> processing_future;

            /// Resize the ring buffer slot to
            static void resize(std::vector<ring_buffer_slot>& buffers,
                               size_t num_channels,
                               enums::codec codec,
                               size_t max_chunk_size,
                               size_t chunk_size_aligned)
            {
                for (auto& slot : buffers)
                {
                    slot.interleaved_buffer.resize(max_chunk_size / sizeof(T));
                    slot.deinterleaved_buffer.resize(num_channels);
                    for (auto& buffer : slot.deinterleaved_buffer)
                    {
                        buffer.resize(chunk_size_aligned / sizeof(T));
                    }

                    if (enums::is_gpu_codec(codec))
                    {
                        slot.memory_pinners.reserve(1 + slot.deinterleaved_buffer.size());
                        slot.memory_pinners.emplace_back(
                            slot.interleaved_buffer.data(),
                            slot.interleaved_buffer.size() * sizeof(T)
                        );
                        for (auto& buffer : slot.deinterleaved_buffer)
                        {
                            slot.memory_pinners.emplace_back(buffer.data(), buffer.size() * sizeof(T));
                        }
                    }
                }
            }

            ring_buffer_slot() = default;
            ring_buffer_slot(ring_buffer_slot&&) noexcept = default;
            ring_buffer_slot& operator=(ring_buffer_slot&&) noexcept = default;
            ring_buffer_slot(const ring_buffer_slot&) = delete;
            ring_buffer_slot& operator=(const ring_buffer_slot&) = delete;
        };

        using ring_buffer_t = std::vector<ring_buffer_slot>;

        /// Ring buffer grows on demand from s_ring_min_slots up to the codec-dependent maximum (see
        /// read_contiguous_channels_impl). We start small to keep the memory/pinning footprint low
        /// and only grow when the producer (file I/O) is observed to stall on the compressors.
        static constexpr size_t s_ring_min_slots = 2;
        /// GPU growth cap: extra in-flight chunks help keep the device fed across the H2D copy /
        /// kernel / D2H pipeline.
        static constexpr size_t s_ring_max_slots_gpu = 6;

        /// Maximum ring slots for a codec. CPU (blosc2) compression is already heavily
        /// multithreaded internally, so a single extra slot is enough to overlap I/O with compute --
        /// more slots add buffers/threads without extra throughput. GPU benefits from more.
        static size_t max_ring_slots(enums::codec codec)
        {
            return enums::is_gpu_codec(codec) ? s_ring_max_slots_gpu : s_ring_min_slots;
        }

    private:
        /// All the channels, each holding their own decompression and compression context.
        std::vector<NAMESPACE_COMPRESSED_IMAGE::channel<T>> m_channels{};

        /// Arbitrary user metadata, not authored or managed by us, it's up to the caller to handle what goes in and comes out
        json_ordered m_metadata{};

        /// Optional set of channelnames to associate to the channels. If not specified sensible defaults are chosen. For example,
        /// if 3 channels are provided we default to { "R", "G", "B" }
        std::vector<std::string> m_channel_names{};

        /// The width of the image file
        size_t m_width = 1;

        /// The height of the image file
        size_t m_height = 1;

    private:
        // Implementations for the read() functions.
        // -----------------------------------------------------------------------------------
        // -----------------------------------------------------------------------------------

#ifdef COMPRESSED_IMAGE_OIIO_AVAILABLE


        /// \brief Read implementation for all the call to image<T>::read().
        ///
        /// This function takes care of reading data from the input pointer and propagating it to read_contiguous_channels_impl.
        ///
        /// \param input_ptr The pointer to read the data from
        /// \param channelnames The channels to read from the file, non-existant channels throw std::out_of_range
        /// \param postprocess An optional postprocessing step to apply to the chunks before they get compressed.
        /// \param compression_codec The compression codec to apply
        /// \param compression_level The compression level to compress with
        /// \param block_size The block size to apply to the compressed data
        /// \param chunk_size The chunk size to apply to the compressed data
        ///
        /// \returns The decoded image.
        template <typename PostProcess = std::nullopt_t>
            requires std::invocable<std::remove_reference_t<PostProcess>, size_t, std::span<T>> || std::is_same_v<
                std::remove_cvref_t<PostProcess>, std::nullopt_t>
        static image read_impl(
            std::unique_ptr<OIIO::ImageInput> input_ptr,
            std::vector<std::string> channelnames,
            PostProcess&& postprocess,
            int subimage,
            enums::codec compression_codec = enums::codec::lz4,
            size_t compression_level = 9,
            size_t block_size = s_default_blocksize,
            size_t chunk_size = s_default_chunksize
        )
        {
            _COMPRESSED_PROFILE_FUNCTION();
            assert(chunk_size % sizeof(T) == 0);
            auto comp_level_adjusted = util::ensure_compression_level(compression_level);

            // Seek to the right subimage before getting the spec.
            auto res = input_ptr->seek_subimage(subimage, 0);
            if (!res)
            {
                throw std::invalid_argument(
                    std::format(
                        "File does not have a subimage {}, cannot seek to it",
                        subimage
                    )
                );
            }
            const OIIO::ImageSpec& spec = input_ptr->spec();

            // Align the chunk size to the scanlines and tiles (if applicable), this makes our life considerably
            // easier and allows us to not deal with partial scanlines.
            size_t chunk_size_aligned = 0;
            if (spec.tile_height != 0)
            {
                chunk_size_aligned = util::align_chunk_to_tile_bytes<T>(spec.width, spec.tile_height, chunk_size);
            }
            else
            {
                chunk_size_aligned = util::align_chunk_to_scanlines_bytes<T>(spec.width, chunk_size);
            }

            // Get a std::vector containing a begin-end pair for all contiguous channels in our channelnames.
            // So if we pass 'R', 'B' and 'A' in a rgba image we would get the following
            // { {0 - 1}, {2 - 4} }
            // This allows us to both maximize performance by handling as many channels in one go as we can while also
            // minimizing memory footprint by only ever allocating as much as we need for the max amount of contiguous
            // channels we can encounter.
            std::vector<NAMESPACE_COMPRESSED_IMAGE::channel<T>> channels;
            auto channel_ranges_contiguous = detail::get_contiguous_channels(input_ptr, channelnames);
            size_t max_num_channels = 0;
            for (const auto& [chbegin, chend] : channel_ranges_contiguous)
            {
                if (static_cast<size_t>(chend) - chbegin > max_num_channels)
                {
                    max_num_channels = static_cast<size_t>(chend) - chbegin;
                }
            }

            // Set up the Ring Buffer (Double Buffering)
            // -----------------------------------------------------------------------------------
            // Start at the minimum number of slots; read_contiguous_channels_impl grows it up to
            // the codec-dependent max on demand. Capacity is reserved up front so that growth (emplace_back)
            // never reallocates the vector -- in-flight compression tasks capture `&slot` by pointer
            // and a reallocation would dangle them.
            const size_t max_chunk_size = chunk_size_aligned * max_num_channels;
            ring_buffer_t ring_buffer;
            ring_buffer.reserve(max_ring_slots(compression_codec));
            ring_buffer.resize(s_ring_min_slots);
            ring_buffer_slot::resize(
                ring_buffer,
                max_num_channels,
                compression_codec,
                max_chunk_size,
                chunk_size_aligned
            );

            // Read and compress the channel pairs in chunks
            // -----------------------------------------------------------------------------------
            // -----------------------------------------------------------------------------------

            // This will be the channelnames we will construct the image with. This is to avoid cases where the user
            // passes the channel names in a different order than they appear in such as 'A', 'G', 'R'. This should
            // still create the channel names as expected in correct order.
            std::vector<std::string> new_channelnames{};

            // Initialize a scratch buffer for compression/decompression.
            auto scratch_buffer = detail::scratch_pool_registry::get_or_create_for_channel();

            // Iterate all the pair and extract them, refitting the buffers as needed.
            // This is where the actual work of reading start.
            for (auto [chbegin, chend] : channel_ranges_contiguous)
            {
                // Calculate some preliminary data for computing how many scanlines to extract in one go.
                int nchannels = chend - chbegin;
                const size_t bytes_per_scanline = static_cast<size_t>(spec.width) * nchannels * sizeof(T);
                const size_t chunk_size_all = chunk_size_aligned * nchannels;
                const size_t scanlines_per_chunk = chunk_size_all / bytes_per_scanline;
                std::vector<detail::schunk<T>> schunks;
                for ([[maybe_unused]] auto _ : std::views::iota(0, nchannels))
                {
                    auto schunk = detail::schunk<T>(block_size, chunk_size_aligned);
                    size_t total_chunks = (spec.height + scanlines_per_chunk - 1) / scanlines_per_chunk;
                    schunk.resize(total_chunks);

                    schunks.push_back(std::move(schunk));
                }

                // Pass the managed ring buffer into our streaming implementation
                if constexpr (std::invocable<std::remove_reference_t<PostProcess>, size_t, std::span<T>>)
                {
                    if (spec.tile_height != 0)
                    {
                        image<T>::template read_contiguous_channels_impl<true>(
                            input_ptr,
                            subimage,
                            chbegin,
                            chend,
                            compression_codec,
                            comp_level_adjusted,
                            block_size,
                            ring_buffer,
                            max_num_channels,
                            max_chunk_size,
                            chunk_size_aligned,
                            scanlines_per_chunk,
                            schunks,
                            std::forward<PostProcess>(postprocess)
                        );
                    }
                    else
                    {
                        image<T>::template read_contiguous_channels_impl<false>(
                            input_ptr,
                            subimage,
                            chbegin,
                            chend,
                            compression_codec,
                            comp_level_adjusted,
                            block_size,
                            ring_buffer,
                            max_num_channels,
                            max_chunk_size,
                            chunk_size_aligned,
                            scanlines_per_chunk,
                            schunks,
                            std::forward<PostProcess>(postprocess)
                        );
                    }
                }
                else
                {
                    if (spec.tile_height != 0)
                    {
                        image<T>::template read_contiguous_channels_impl<true>(
                            input_ptr,
                            subimage,
                            chbegin,
                            chend,
                            compression_codec,
                            comp_level_adjusted,
                            block_size,
                            ring_buffer,
                            max_num_channels,
                            max_chunk_size,
                            chunk_size_aligned,
                            scanlines_per_chunk,
                            schunks,
                            std::nullopt
                        );
                    }
                    else
                    {
                        image<T>::template read_contiguous_channels_impl<false>(
                            input_ptr,
                            subimage,
                            chbegin,
                            chend,
                            compression_codec,
                            comp_level_adjusted,
                            block_size,
                            ring_buffer,
                            max_num_channels,
                            max_chunk_size,
                            chunk_size_aligned,
                            scanlines_per_chunk,
                            schunks,
                            std::nullopt
                        );
                    }
                }

                for (const auto channel_idx : std::views::iota(0, nchannels))
                {
                    _COMPRESSED_PROFILE_SCOPE("generate channels");
                    channels.push_back(
                        NAMESPACE_COMPRESSED_IMAGE::channel<T>(
                            std::move(schunks[channel_idx]),
                            spec.width,
                            spec.height,
                            compression_codec,
                            comp_level_adjusted
                        )
                    );
                }

                for (auto channel_idx : std::views::iota(chbegin, chend))
                {
                    new_channelnames.push_back(spec.channelnames.at(channel_idx));
                }
            }

            auto img = NAMESPACE_COMPRESSED_IMAGE::image<T>(
                std::move(channels),
                spec.width,
                spec.height,
                new_channelnames
            );
            img.metadata(NAMESPACE_COMPRESSED_IMAGE::image<T>::read_oiio_metadata(spec));
            return std::move(img);
        }


        /// \brief Read a contiguous channel sequence from the passed input pointer
        ///
        /// When reading with OpenImageIO it is a lot more efficient to parse as many channels as possible in one go
        /// rather than reading one channel at a time as the ImageInput keeps the data as compressed (in many cases).
        /// If we were to read one channel at a time this would significantly slow down our read speeds.
        ///
        /// Due to us only being able to read contiguous channels at a time this helper function allows us to do that.
        ///
        /// \param input_ptr The opened OpenImageIO ImageInput.
        /// \param subimage The subimage to read
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
        template <bool read_tiles, typename PostProcess = std::nullopt_t>
            requires std::invocable<std::remove_reference_t<PostProcess>, size_t, std::span<T>> || std::is_same_v<
                std::remove_cvref_t<PostProcess>, std::nullopt_t>
        static void read_contiguous_channels_impl(
            std::unique_ptr<OIIO::ImageInput>& input_ptr,
            const int subimage,
            const int chbegin,
            const int chend,
            const enums::codec compression_codec,
            const size_t compression_level,
            const size_t block_size,
            ring_buffer_t& ring_buffer,
            size_t max_num_channels,
            size_t max_chunk_size,
            size_t chunk_size_aligned,
            size_t scanlines_per_chunk,
            std::vector<detail::schunk<T>>& schunks,
            PostProcess&& postprocess
        )
        {
            _COMPRESSED_PROFILE_FUNCTION();

            // Keep the pool sized to the number of ring slots, and grow it in lockstep when the ring
            // grows (below). The compression tasks run on cudaStreamPerThread, so each worker thread
            // owns its own CUDA stream; sizing the pool to the actual concurrency (= number of slots)
            // keeps each in-flight chunk on a stable, reused stream. An oversized pool would scatter
            // the same work across more threads/streams, churning per-thread CUDA state.
            BS::thread_pool thread_pool(ring_buffer.size());

            const int nchannels = chend - chbegin;
            assert(input_ptr->current_subimage() == subimage);
            const OIIO::ImageSpec& spec = input_ptr->spec();
            const auto typedesc = enums::get_type_desc<T>();

            // Ensure this function is called with at least 1 channel to read.
            if (nchannels < 1)
            {
                throw std::runtime_error(
                    std::format(
                        "read_contiguous_channels_impl: passed number of channels is less than one. This should not happen. Got {}",
                        nchannels
                    )
                );
            }

            size_t ring_index = 0;
            size_t current_chunk_id = 0;
            int y = spec.y;

            while (y < (spec.height + spec.y))
            {
                _COMPRESSED_PROFILE_SCOPE("Read Scanlines/Tiles and compress");
                int scanlines_to_read = static_cast<int>(std::min<size_t>(
                    scanlines_per_chunk,
                    static_cast<size_t>(spec.height + spec.y - y)
                ));

                // Select active slot in our ring buffer
                auto& slot = ring_buffer[ring_index];

                // 1. Wait if this slot's own previous turn hasn't finished (Safe Guard for small ring buffers).
                //    The time we block here is the "read gap": the producer has lapped the ring and is
                //    waiting on the background compressors. We measure it to decide whether to grow.
                const auto wait_start = std::chrono::steady_clock::now();
                if (slot.processing_future.valid())
                {
                    slot.processing_future.get();
                }
                const auto read_gap = std::chrono::steady_clock::now() - wait_start;

                // Slice out exact span dimensions required for the OIIO validation checks and bounds
                const size_t chunk_size_all = scanlines_per_chunk * spec.width * nchannels;
                auto interleaved_fitted = std::span<T>(slot.interleaved_buffer.data(), chunk_size_all);

                // 2. STAGE 1 (I/O): Synchronously read next file chunk on main thread
                const auto read_start = std::chrono::steady_clock::now();
                bool read_successful = false;
                if constexpr (read_tiles)
                {
                    _COMPRESSED_PROFILE_SCOPE("read tiles");
                    read_successful = input_ptr->read_tiles(
                        subimage,
                        0,
                        spec.x,
                        spec.width,
                        y,
                        y + scanlines_to_read,
                        0,
                        1,
                        chbegin,
                        chend,
                        typedesc,
                        static_cast<void*>(interleaved_fitted.data())
                    );
                }
                else
                {
                    _COMPRESSED_PROFILE_SCOPE("read scanlines");
                    read_successful = input_ptr->read_scanlines(
                        subimage,
                        0,
                        y,
                        y + scanlines_to_read,
                        0,
                        chbegin,
                        chend,
                        typedesc,
                        static_cast<void*>(interleaved_fitted.data())
                    );
                }

                if (!read_successful)
                {
                    throw std::runtime_error(
                        std::format(
                            "OIIO read failure when reading scanlines {}-{} for channels {}-{}: '{}'",
                            y,
                            y + scanlines_to_read,
                            chbegin,
                            chend,
                            input_ptr->geterror()
                        )
                    );
                }
                const auto read_duration = std::chrono::steady_clock::now() - read_start;

                // Grow the ring buffer (up to max_ring_slots for this codec) when the producer is stalling on the
                // compressors: if we blocked longer waiting for a free slot than this read took, more
                // in-flight slots let I/O and compression overlap further. Reserved capacity means the
                // emplace never reallocates, so `slot` and any in-flight `&slot` captures stay valid.
                if (ring_buffer.size() < max_ring_slots(compression_codec) && read_gap > read_duration / 2)
                {
                    std::vector<ring_buffer_slot> grown(1);
                    ring_buffer_slot::resize(
                        grown,
                        max_num_channels,
                        compression_codec,
                        max_chunk_size,
                        chunk_size_aligned
                    );
                    ring_buffer.emplace_back(std::move(grown[0]));

                    // Grow the pool in lockstep so each concurrent chunk keeps its own worker thread
                    // (and thus its own cudaStreamPerThread). reset() drains the in-flight tasks
                    // first -- a brief barrier, but we only grow a couple of times and only while
                    // already stalling, so it pays for itself.
                    thread_pool.reset(ring_buffer.size());
                }

                size_t read_elements = static_cast<size_t>(scanlines_to_read) * spec.width;
                const size_t channel_width = static_cast<size_t>(spec.width);

                // 4. STAGE 2 (COMPUTE): Delegate processing & compression of the freshly read chunk to a background task.
                // Main thread loops back immediately to read chunk k+1 into the alternate buffer slot.
                slot.processing_future = thread_pool.submit_task(
                    [
                        &slot, interleaved_fitted, nchannels, read_elements, compression_codec, compression_level,
                        block_size, chbegin, channel_width,
                        &schunks, &postprocess, current_chunk_id
                    ]()
                    {
                        // Slice deinterleaved spans for this active task
                        std::vector<std::span<T>> deinterleaved_fitted_views;
                        deinterleaved_fitted_views.reserve(nchannels);
                        for (int idx = 0; idx < nchannels; ++idx)
                        {
                            deinterleaved_fitted_views.emplace_back(
                                slot.deinterleaved_buffer[idx].data(),
                                slot.deinterleaved_buffer[idx].size()
                            );
                        }

                        // Compute steps
                        image_algo::deinterleave(std::span<const T>(interleaved_fitted), deinterleaved_fitted_views);

                        for (auto channel_idx : std::views::iota(0, nchannels))
                        {
                            auto context = NAMESPACE_COMPRESSED_IMAGE::channel<T>::create_compression_context(
                                compression_codec,
                                std::thread::hardware_concurrency(),
                                compression_level,
                                block_size,
                                0,
                                channel_width
                            );

                            auto channel_span = std::span<T>(
                                slot.deinterleaved_buffer[channel_idx].data(),
                                read_elements
                            );

                            if constexpr (std::invocable<std::remove_reference_t<PostProcess>, size_t, std::span<T>>)
                            {
                                auto absolute_channel_idx = chbegin + channel_idx;
                                postprocess(absolute_channel_idx, channel_span);
                            }

                            schunks[channel_idx].set_chunk(
                                std::move(context),
                                channel_span,
                                current_chunk_id,
                                false /* validate_chunk_sizes */
                            );
                        }
                    }
                );

                current_chunk_id++;
                ring_index = (ring_index + 1) % ring_buffer.size();
                y += scanlines_to_read;
            }

            for (auto& slot : ring_buffer)
            {
                if (slot.processing_future.valid())
                {
                    try
                    {
                        slot.processing_future.get();
                    }
                    catch (const std::exception& e)
                    {
                        get_logger()->error(std::format("Exception caught from async thread: {}", e.what()));
                        throw;
                    }
                }
            }

            // Now that all threads have safely populated the vectors, validate the sizes
            for (auto& schunk : schunks)
            {
                schunk.validate_chunk_sizes();
            }
        }


#endif // COMPRESSED_IMAGE_OIIO_AVAILABLE
    };
} // NAMESPACE_COMPRESSED_IMAGE
