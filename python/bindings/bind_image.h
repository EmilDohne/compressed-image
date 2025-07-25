#pragma once

#include "wrappers/dynamic_channel.h"
#include "wrappers/dynamic_image.h"

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace compressed_py
{

	void bind_compressed_image(py::module_& m)
    {

        py::class_<compressed_py::dynamic_image, std::shared_ptr<compressed_py::dynamic_image>>(m, "Image", R"doc(
A dynamically-typed compressed image composed of multiple channels, supporting
interaction with numpy arrays and efficient memory/storage via lazy compression.

Supports the following np.dtypes as fill values:
    - np.float16
    - np.float32
    - np.uint8
    - np.int8
    - np.uint16
    - np.int16
    - np.uint32
    - np.int32

The channels are stored as compressed buffers allowing for efficient traversal and decompression.
        )doc")
            .def(py::init<
                    const py::object&,
                    std::vector<py::array>,
                    size_t,
                    size_t,
                    std::vector<std::string>,
                    compressed::enums::codec,
                    size_t,
                    size_t,
                    size_t>(),
                py::arg("dtype"),
                py::arg("channels"),
                py::arg("width"),
                py::arg("height"),
                py::arg("channel_names") = std::vector<std::string>{},
                py::arg("compression_codec") = compressed::enums::codec::lz4,
                py::arg("compression_level") = 9,
                py::arg("block_size") = compressed::s_default_blocksize,
                py::arg("chunk_size") = compressed::s_default_chunksize,
                R"doc(
Construct a compressed image from a list of numpy arrays, converting the arrays
into compressed_image.Channel instances. The compresion settings can be controlled with 
the `compression_codec`, `compression_level`, `block_size`, `chunk_size` parameters.

:param dtype: The dtype of the image data.
:param channels: List of 2D numpy arrays representing image channels. These should have
                 the shape (height, width) storing the image data in scanline order.
:param width: Width of the image.
:param height: Height of the image.
:param channel_names: Optional list of channel names to assign.
:param compression_codec: Compression codec, defaults to lz4 which is a good compromise 
                          between compression ratio and speed usually achieving between 5-10x
                          compression ratios.
:param compression_level: Compression level, defaults to 9 for lz4 and typically ranges between 
                          0-9. For lz4 9 is usually a good value as performance doesn't suffer 
                          much from having this at its highest.
:param block_size: The block size used internally in the 3d containers, defaults to 32KB which,
                   from testing, appears to be a good all-purpose value for both performance and
                   compression ratio. This is the size of a single run of data to be compressed
                   and should roughly fit into the L1 cache of your CPU for best performance.
                   The blocks are not transparent to the user of the API, the lowest level that
                   can be accessed is `chunks`.
:param chunk_size: Chunk size used internally (next level after blocks). Defaults to `4 * 1024 * 1024`.

                   This value should be:
                   
                   - **Small enough** to allow efficient partial extraction (e.g., for region-of-interest).
                   - **Large enough** to saturate available cores when divided by `block_size`.
                   
                   These defaults are tuned for good performance on a wide range of systems.
            )doc")
            .def_static("read",
                py::overload_cast<
                    const py::object&,
                    std::string,
                    int,
                    compressed::enums::codec,
                    size_t,
                    size_t,
                    size_t>(&compressed_py::dynamic_image::read),
                py::arg("dtype"),
                py::arg("filepath"), 
                py::arg("subimage"),
                py::arg("compression_codec") = compressed::enums::codec::lz4,
                py::arg("compression_level") = 9,
                py::arg("block_size") = compressed::s_default_blocksize,
                py::arg("chunk_size") = compressed::s_default_chunksize,
                R"doc(
Reads the specified image from disk, converting into the passed dtype and compressing on the fly.
This method is much more memory efficient and faster than doing this step yourself (reading 
and then passing read data to compressed_image.Image) as it reads in chunks and compressed on
the fly.

All the channels at the given subimage are read, if multiple subimages are to be read it is
recommended to split this into several compressed_image.Image for the above mentioned reasons.

This method automatically populates the metadata that can be accessed through `self.get_metadata()`
with the image metadata.

:param dtype: The data type to read as, this doesn't have to correspond to the data type of the
              image as we will convert to the data on read. If you wish to find out the data type
              of an image without having to read it you can use the Image.dtype_from_file method.
:param filepath: The path to the image file, this must be in a format supported by OpenImageIO.
:param subimage: The subimage within the image to read. Only relevant for a couple of formats such
                 as tiff or exr. If you need to find out how to read the subimage information from
                 an image please refer to the docs for OpneImageIO.
:param compression_codec: Compression codec, defaults to lz4 which is a good compromise 
                          between compression ratio and speed usually achieving between 5-10x
                          compression ratios.
:param compression_level: Compression level, defaults to 9 for lz4 and typically ranges between 
                          0-9. For lz4 9 is usually a good value as performance doesn't suffer 
                          much from having this at its highest.
:param block_size: The block size used internally in the 3d containers, defaults to 32KB which,
                   from testing, appears to be a good all-purpose value for both performance and
                   compression ratio. This is the size of a single run of data to be compressed
                   and should roughly fit into the L1 cache of your CPU for best performance.
                   The blocks are not transparent to the user of the API, the lowest level that
                   can be accessed is `chunks`.
:param chunk_size: Chunk size used internally (next level after blocks). Defaults to `4 * 1024 * 1024`.

                   This value should be:
                   
                   - **Small enough** to allow efficient partial extraction (e.g., for region-of-interest).
                   - **Large enough** to saturate available cores when divided by `block_size`.
                   
                   These defaults are tuned for good performance on a wide range of systems.
            )doc")

            .def_static("read",
                py::overload_cast<
                    const py::object&,
                    std::string,
                    int,
                    std::vector<int>,
                    compressed::enums::codec,
                    size_t,
                    size_t,
                    size_t>(&compressed_py::dynamic_image::read),
                py::arg("dtype"),
                py::arg("filepath"), 
                py::arg("subimage"), 
                py::arg("channel_indices"),
                py::arg("compression_codec") = compressed::enums::codec::lz4,
                py::arg("compression_level") = 9,
                py::arg("block_size") = compressed::s_default_blocksize,
                py::arg("chunk_size") = compressed::s_default_chunksize,
                R"doc(
Reads the specified image from disk, converting into the passed dtype and compressing on the fly.
This method is much more memory efficient and faster than doing this step yourself (reading 
and then passing read data to compressed_image.Image) as it reads in chunks and compressed on
the fly.

The channels specified by `channel_indices` are read from `subimage`, if multiple subimages are to be read it is
recommended to split this into several compressed_image.Image for the above mentioned reasons.

This method automatically populates the metadata that can be accessed through `self.get_metadata()`
with the image metadata.

:param dtype: The data type to read as, this doesn't have to correspond to the data type of the
              image as we will convert to the data on read. If you wish to find out the data type
              of an image without having to read it you can use the Image.dtype_from_file method.
:param filepath: The path to the image file, this must be in a format supported by OpenImageIO.
:param subimage: The subimage within the image to read. Only relevant for a couple of formats such
                 as tiff or exr. If you need to find out how to read the subimage information from
                 an image please refer to the docs for OpneImageIO.
:param channel_indices: The indices of the channels to read. These must be valid for the passed image
                        and subimage as otherwise we will throw an exception.
:param compression_codec: Compression codec, defaults to lz4 which is a good compromise 
                          between compression ratio and speed usually achieving between 5-10x
                          compression ratios.
:param compression_level: Compression level, defaults to 9 for lz4 and typically ranges between 
                          0-9. For lz4 9 is usually a good value as performance doesn't suffer 
                          much from having this at its highest.
:param block_size: The block size used internally in the 3d containers, defaults to 32KB which,
                   from testing, appears to be a good all-purpose value for both performance and
                   compression ratio. This is the size of a single run of data to be compressed
                   and should roughly fit into the L1 cache of your CPU for best performance.
                   The blocks are not transparent to the user of the API, the lowest level that
                   can be accessed is `chunks`.
:param chunk_size: Chunk size used internally (next level after blocks). Defaults to `4 * 1024 * 1024`.

                   This value should be:
                   
                   - **Small enough** to allow efficient partial extraction (e.g., for region-of-interest).
                   - **Large enough** to saturate available cores when divided by `block_size`.
                   
                   These defaults are tuned for good performance on a wide range of systems.
            )doc")

            .def_static("read",
                py::overload_cast<
                    const py::object&,
                    std::string,
                    int,
                    std::vector<std::string>,
                    compressed::enums::codec,
                    size_t,
                    size_t,
                    size_t>(&compressed_py::dynamic_image::read),
                py::arg("dtype"),
                py::arg("filepath"), 
                py::arg("subimage"), 
                py::arg("channel_names"),
                py::arg("compression_codec") = compressed::enums::codec::lz4,
                py::arg("compression_level") = 9,
                py::arg("block_size") = compressed::s_default_blocksize,
                py::arg("chunk_size") = compressed::s_default_chunksize,
                R"doc(
Reads the specified image from disk, converting into the passed dtype and compressing on the fly.
This method is much more memory efficient and faster than doing this step yourself (reading 
and then passing read data to compressed_image.Image) as it reads in chunks and compressed on
the fly.

The channels specified by `channel_names` are read from `subimage`, if multiple subimages are to be read it is
recommended to split this into several compressed_image.Image for the above mentioned reasons.

This method automatically populates the metadata that can be accessed through `self.get_metadata()`
with the image metadata.

:param dtype: The data type to read as, this doesn't have to correspond to the data type of the
              image as we will convert to the data on read. If you wish to find out the data type
              of an image without having to read it you can use the Image.dtype_from_file method.
:param filepath: The path to the image file, this must be in a format supported by OpenImageIO.
:param subimage: The subimage within the image to read. Only relevant for a couple of formats such
                 as tiff or exr. If you need to find out how to read the subimage information from
                 an image please refer to the docs for OpneImageIO.
:param channel_names: The names of the channels to read. These must be valid for the passed image
                      and subimage as otherwise we will throw an exception.
:param compression_codec: Compression codec, defaults to lz4 which is a good compromise 
                          between compression ratio and speed usually achieving between 5-10x
                          compression ratios.
:param compression_level: Compression level, defaults to 9 for lz4 and typically ranges between 
                          0-9. For lz4 9 is usually a good value as performance doesn't suffer 
                          much from having this at its highest.
:param block_size: The block size used internally in the 3d containers, defaults to 32KB which,
                   from testing, appears to be a good all-purpose value for both performance and
                   compression ratio. This is the size of a single run of data to be compressed
                   and should roughly fit into the L1 cache of your CPU for best performance.
                   The blocks are not transparent to the user of the API, the lowest level that
                   can be accessed is `chunks`.
:param chunk_size: Chunk size used internally (next level after blocks). Defaults to `4 * 1024 * 1024`.

                   This value should be:
                   
                   - **Small enough** to allow efficient partial extraction (e.g., for region-of-interest).
                   - **Large enough** to saturate available cores when divided by `block_size`.
                   
                   These defaults are tuned for good performance on a wide range of systems.
            )doc")

            .def_static("dtype_from_file", &compressed_py::dynamic_image::dtype_from_file,
                py::arg("filepath"),
                R"doc(
Extract the dtype from a given image file without having to read the whole image. This is the 
recommended method for extracting the files' dtype from an image.

:returns: The dtype of the image at the given filepath.
            )doc")

        .def_static("dtypes_from_file", &compressed_py::dynamic_image::dtypes_from_file,
        py::arg("filepath"),
        R"doc(
Extract all the dtypes from a given image file without having to read the whole image. This is the 
recommended method for extracting the files' dtype from an image. This will either extract a list of all dtypes
of the image with each entry corresponding to the a channel or a list of a single value with the dtype for
the whole image if the image does not have per-channel dtypes.

So in an image with these channels

["R", "G", "B", "A", "Z"]

it might be like this:

[np.float16, np.float16, np.float16, np.float16, np.float32]

:returns: The dtypes of the image at the given filepath.
            )doc")

            .def("add_channel", &compressed_py::dynamic_image::add_channel,
                py::arg("data"),
                py::arg("width"),
                py::arg("height"),
                py::arg("name") = std::nullopt,
                py::arg("compression_codec") = compressed::enums::codec::lz4,
                py::arg("compression_level") = 9,
                py::arg("block_size") = compressed::s_default_blocksize,
                py::arg("chunk_size") = compressed::s_default_chunksize,
                R"doc(
Add a channel to the image from the given uncompressed data. Compresses it and stores it in `self.channels`.

:param data: The image data to add as the channel, this needs to both have the same dtype as `self.dtype`
             as well as having to have the shape of `(height, width)`.
:param width: The width of the channel, must be the same as the rest of the image.
:param height: The height of the channel, must be the same as the rest of the image.
:param compression_codec: Compression codec, defaults to lz4 which is a good compromise 
                          between compression ratio and speed usually achieving between 5-10x
                          compression ratios.
:param compression_level: Compression level, defaults to 9 for lz4 and typically ranges between 
                          0-9. For lz4 9 is usually a good value as performance doesn't suffer 
                          much from having this at its highest.
:param block_size: The block size used internally in the 3d containers, defaults to 32KB which,
                   from testing, appears to be a good all-purpose value for both performance and
                   compression ratio. This is the size of a single run of data to be compressed
                   and should roughly fit into the L1 cache of your CPU for best performance.
                   The blocks are not transparent to the user of the API, the lowest level that
                   can be accessed is `chunks`.
:param chunk_size: Chunk size used internally (next level after blocks). Defaults to `4 * 1024 * 1024`.

                   This value should be:
                   
                   - **Small enough** to allow efficient partial extraction (e.g., for region-of-interest).
                   - **Large enough** to saturate available cores when divided by `block_size`.
                   
                   These defaults are tuned for good performance on a wide range of systems.
            )doc")

            .def("remove_channel", &compressed_py::dynamic_image::remove_channel,
                py::arg("name_or_index"),
                R"doc(
Remove the channel by name or index.
            )doc")

            .def("__getitem__", [](std::shared_ptr<compressed_py::dynamic_image>& self, std::variant<std::string, size_t> key)
            {
                if (std::holds_alternative<size_t>(key))
                {
                    return self->channel(std::get<size_t>(key));
                }
                else
                {
                    return self->channel(std::get<std::string>(key));
                }

			}, py::arg("key"), R"doc(
Retrieve a channel by name or index.
            )doc")

            .def("__len__", [](std::shared_ptr<compressed_py::dynamic_image>& self)
            {
                return self->num_channels();
            })

            .def("channel", py::overload_cast<size_t>(&compressed_py::dynamic_image::channel),
                py::arg("index"),
                R"doc(
Retrieve a channel by its logical index

:param index: The index of the channel, must be valid

:returns: The channel, this may be modified in-place and will be updated on the containing image.
            )doc")

            .def("channel", py::overload_cast<const std::string_view>(&compressed_py::dynamic_image::channel),
                py::arg("name"),
                R"doc(
Retrieve a channel by its name

:param name: The name of the channel, must be valid

:returns: The channel, this may be modified in-place and will be updated on the containing image.
            )doc")

            .def("channels", &compressed_py::dynamic_image::channels,
                R"doc(
Retrieve a reference to all the channels of the image, the compressed_image.Channel instances may be modified 
directly but modifying the list will not update the channels of the image. To add or remove a channel please call
`add_channel` or `remove_channel`.
            )doc")

            .def("get_decompressed", &compressed_py::dynamic_image::get_decompressed,
                R"doc(
Retrieve all the channels as a list of numpy.ndarrays. These are guaranteed to be in the same order as the
underlying channels so it is safe to access these by their logical index.
            )doc")

            .def("get_channel_index", &compressed_py::dynamic_image::get_channel_index,
                py::arg("channelname"),
                R"doc(
Return the index of a channel by its name, raising a ValueError if the name is not valid.
            )doc")

            .def("print_statistics", &compressed_py::dynamic_image::print_statistics,
                R"doc(
Prints some general image statistics such as compression ratio, channel names, bytesize etc.

Example output:

    Statistics for image buffer:
    Width:             1024
    Height:            768
    Channels:          3
    Channelnames:      [R, G, B]
    --------------
    Compressed Size:   123456 bytes
    Uncompressed Size: 3145728 bytes
    Compression ratio: 25.5x
    Num Chunks:        512
    Metadata:
    {
    "author": "User",
    "timestamp": "2024-03-15"
    }
            )doc")

            .def("compression_ratio", &compressed_py::dynamic_image::compression_ratio,
                R"doc(
:return: The compression ratio of the data.
            )doc")

			.def_property_readonly("shape", &compressed_py::dynamic_image::shape,
			R"doc(
:return: Image shape in format (nchannels, height, width)
            )doc")

            .def_property_readonly("width", &compressed_py::dynamic_image::width,
                R"doc(
:return: The image width. All channels in the image are guaranteed to be of this width.
            )doc")

            .def_property_readonly("height", &compressed_py::dynamic_image::height,
                R"doc(
:return: The image height. All channels in the image are guaranteed to be of this height.
            )doc")

            .def_property_readonly("num_channels", &compressed_py::dynamic_image::num_channels,
                R"doc(
:return: Number of channels. Equivalent to calling len(image)
            )doc")

            .def("get_channel_names", py::overload_cast<>(&compressed_py::dynamic_image::channel_names, py::const_), R"doc(
Retrieve the channel names, these aren't guaranteed to be populated if e.g. the image doesn't have any channel names
assigned to them on creation. This is always guaranteed however to be either empty or the size of self.num_channels
            )doc")
            .def("set_channel_names", py::overload_cast<std::vector<std::string>>(&compressed_py::dynamic_image::channel_names), 
                R"doc(
Set the channel names, the names of these may be whatever you wish the to be, the only restriction is that the length of this
list must be == len(channel).
            )doc")

            .def("update_nthreads", &compressed_py::dynamic_image::update_nthreads,
                py::arg("nthreads"),
                R"doc(
Update the number of threads used internally for compression/decompression. By default this will use all available system threads.
            )doc")

            .def("chunk_size", &compressed_py::dynamic_image::chunk_size,
                R"doc(
Get the chunk size used for compression, this is the same across all channels.
            )doc")
            .def("set_metadata", &compressed_py::dynamic_image::set_metadata, py::arg("metadata"), R"doc(
Set the metadata on the image, this may be any arbitrary dict and it is entirely up to you to manage this metadata/interpret it.
This may for example encode color space information.

:param metadata: The metadata to set on the image.
            )doc"
            )
            .def("get_metadata", &compressed_py::dynamic_image::get_metadata, R"doc(
Retrieve the metadata stored on the image. If the image was created via one of the `read()` methods this will be populated with 
the image metadata which will store additional information.
            )doc");
    }

} // compressed_py