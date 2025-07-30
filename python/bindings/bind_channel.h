#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#include "wrappers/dynamic_channel.h"

namespace py = pybind11;


namespace compressed_py
{

    void bind_compressed_channel(py::module_& m)
    {

        py::class_<compressed_py::dynamic_channel, std::shared_ptr<compressed_py::dynamic_channel>>(m, "Channel", R"doc(
A dynamically-typed compressed image channel with support for lazy-storage.

Provides compressed image data with access to shape, compression settings,
and conversion to/from numpy arrays.
    
Supports the following np.dtypes as fill values:
    - np.float16
    - np.float32
    - np.uint8
    - np.int8
    - np.uint16
    - np.int16
    - np.uint32
    - np.int32

The data is stored as compressed chunks rather than as one large compressed array allowing
for decompression/recompression of only parts of the data allowing for very memory-efficient 
operations.
        )doc")
            .def(py::init<py::array,
                size_t,
                size_t,
                compressed::enums::codec,
                uint8_t,
                size_t,
                size_t>(),
                py::arg("data"),
                py::arg("width"),
                py::arg("height"),
                py::arg("compression_codec") = compressed::enums::codec::lz4,
                py::arg("compression_level") = 9,
                py::arg("block_size") = compressed::s_default_blocksize,
                py::arg("chunk_size") = compressed::s_default_chunksize,
                R"doc(
Initialize a compressed channel from a numpy array with the given compression settings. This numpy array
should be 1/2-dimensional with it's overall size matching width * height. 

Typically you do not need to modify any of the defaults for `compression_codec`, `compression_level`, `block_size` and
`chunk_size`

:param data: The input numpy array, must be 1- or 2-dimensional.
:param width: Width of the channel.
:param height: Height of the channel.
:param compression_codec: Compression codec to use (default: lz4).
:param compression_level: Compression level (default: 9).
:param block_size: Block size for compression (default: 32_768).
:param chunk_size: Chunk size for compression (default: 4_194_304).
            )doc")
            .def_static("full", &compressed_py::dynamic_channel::full,
                py::arg("dtype"), 
                py::arg("fill_value"),
                py::arg("width"), 
                py::arg("height"),
                py::arg("compression_codec") = compressed::enums::codec::lz4,
                py::arg("compression_level") = 9,
                py::arg("block_size") = compressed::s_default_blocksize,
                py::arg("chunk_size") = compressed::s_default_chunksize,
                R"doc(
Create a new lazy-channel initialized with the fill value. This is very efficient
as it only stores a single value per-chunk only storing compressed data for it if explicitly
done so via `set_chunk`

:param dtype: numpy dtype for the data.
:param fill_value: The fill value for the data, may be a float or an integer
:param width: Image width.
:param height: Image height.
:param compression_codec: Compression codec.
:param compression_level: Compression level.
:param block_size: Block size for compression.
:param chunk_size: Chunk size for compression.
:return: A new compressed_image.Channel.
            )doc")
            .def_static("zeros", &compressed_py::dynamic_channel::zeros,
                py::arg("dtype"), 
                py::arg("width"), 
                py::arg("height"),
                py::arg("compression_codec") = compressed::enums::codec::lz4,
                py::arg("compression_level") = 9,
                py::arg("block_size") = compressed::s_default_blocksize,
                py::arg("chunk_size") = compressed::s_default_chunksize,
                R"doc(
Create a new channel filled with zeros. This is very efficient
as it only stores a single value per-chunk only storing compressed data for it if explicitly
done so via `set_chunk`

:param dtype: numpy dtype for the data.
:param width: Image width.
:param height: Image height.
:param compression_codec: Compression codec.
:param compression_level: Compression level.
:param block_size: Block size for compression.
:param chunk_size: Chunk size for compression.
:return: A new `compressed_image.Channel`.
            )doc")
            .def_static("full_like", &compressed_py::dynamic_channel::full_like,
                py::arg("other"),
                py::arg("fill_value"),
                R"doc(
Create a new channel with the same shape and dtype as another, filled to `fill_value`.

:param other: Another `compressed_image.Channel` to mimic.
:param fill_value: The fill value for the data, may be a float or an integer
:return: A new `compressed_image.Channel`.
            )doc")
            .def_static("zeros_like", &compressed_py::dynamic_channel::zeros_like,
                py::arg("other"),
                R"doc(
Create a new channel with the same shape and dtype as another, filled with zeros.

:param other: Another `compressed_image.Channel` to mimic.
:return: A new `compressed_image.Channel`.
            )doc")
            .def_property_readonly("dtype", &compressed_py::dynamic_channel::dtype, R"doc(
:return: The numpy dtype of the underlying data.
            )doc")
            .def_property_readonly("shape", &compressed_py::dynamic_channel::shape,
                R"doc(
:return: Tuple of (height, width).
            )doc")
            .def_property_readonly("width", &compressed_py::dynamic_channel::width)
            .def_property_readonly("height", &compressed_py::dynamic_channel::height)
                    .def("compressed_bytes", &compressed_py::dynamic_channel::compressed_bytes,
                        R"doc(
:return: Size of the compressed data in bytes.
            )doc")
                    .def("uncompressed_size", &compressed_py::dynamic_channel::uncompressed_size,
                        R"doc(
:return: Number of elements in the uncompressed array.
            )doc")
                    .def("num_chunks", &compressed_py::dynamic_channel::num_chunks,
                        R"doc(
:return: Number of chunks in the compressed channel.
            )doc")
                    .def("block_size", &compressed_py::dynamic_channel::block_size,
                        R"doc(
:return: Block size used for compression.
            )doc")
                    .def("chunk_size",
                        py::overload_cast<>(&compressed_py::dynamic_channel::chunk_size, py::const_),
                        R"doc(
:return: Chunk size (bytes) used for compression.
            )doc")
			        .def("chunk_elems",
					py::overload_cast<>(&compressed_py::dynamic_channel::chunk_elems, py::const_),
					R"doc(
:return: Number of elements in a single chunk
            )doc")
                    .def("chunk_size",
                        py::overload_cast<size_t>(&compressed_py::dynamic_channel::chunk_size, py::const_),
                        py::arg("chunk_index"),
                        R"doc(
:param chunk_index: Index of the chunk.
:return: Size of the specified chunk (bytes).
            )doc")
			        .def("chunk_elems",
			            py::overload_cast<size_t>(&compressed_py::dynamic_channel::chunk_elems, py::const_),
                        py::arg("chunk_index"),
		            	R"doc(
:param chunk_index: Index of the chunk.
:return: Number of elements in a single chunk
            )doc")
                    .def("get_chunk", 
                        py::overload_cast<size_t>(&compressed_py::dynamic_channel::get_chunk, py::const_),
                        py::arg("chunk_index"),
                        R"doc(
Get the decompressed data for a chunk. This represents a sub-part of the image which may be aligned to 
scanlines, but it doesn't have to be. To compute the starting coordinate of a chunk you should query

start_x = chunk_index * channel.chunk_size() % channel.width()
start_y = chunk_index * channel.chunk_size() // channel.width()

from there you can compute the full extents of the chunk.

:param chunk_index: Index of the chunk to decompress.
:return: 1D numpy array containing decompressed data.
            )doc")

			        .def("get_chunk",
			            py::overload_cast<size_t, py::array&>(&compressed_py::dynamic_channel::get_chunk, py::const_),
			            py::arg("chunk_index"), py::arg("array"),
			            R"doc(
Get the decompressed data for a chunk. This represents a sub-part of the image which may be aligned to 
scanlines, but it doesn't have to be. To compute the starting coordinate of a chunk you should query

start_x = chunk_index * channel.chunk_size() % channel.width()
start_y = chunk_index * channel.chunk_size() // channel.width()

from there you can compute the full extents of the chunk.

This overload allows you to reuse a buffer rather than having to keep setting up a new one, to use it you should so 
something along the lines of the below code example. A channels chunks are guaranteed to be the same size for all chunks
except the last one, so we can reuse the same buffer again.

.. code-block:: python

    buffer = np.ndarray((channel.chunk_elems(),), dtype= channel.dtype)
    for i in range(channel.num_chunks() - 1):
        channel.get_chunk(i, buffer)
        # Modify chunk
        channel.set_chunk(i, buffer)

    final_buffer = channel.get_chunk(channel.num_chunks() -1)
    # Modify chunk
    channel.set_chunk(channel.num_chunks() -1, final_buffer)

:param chunk_index: Index of the chunk to decompress.
:param array: The 1D numpy array to extract the data to, must be exactly `chunk_elems(chunk_index)` in size.
:return: 1D numpy array containing decompressed data.
            )doc")
                    .def("set_chunk", &compressed_py::dynamic_channel::set_chunk,
                        py::arg("chunk_index"), py::arg("array"),
                        R"doc(
Replace a chunk's contents with a new array. This array must match the size `channel.chunk_size(chunk_index)`.

:param chunk_index: Index of the chunk to update. Must be less than self.num_chunks
:param array: 1D numpy array to set onto the chunk.
            )doc")
                    .def("get_decompressed", &compressed_py::dynamic_channel::get_decompressed,
                        R"doc(
:return: The full decompressed data as a 2D numpy array.
            )doc")
                    .def("update_nthreads", &compressed_py::dynamic_channel::update_nthreads,
                        py::arg("nthreads"), py::arg("block_size") = compressed::s_default_blocksize,
                        R"doc(
Update the number of threads used for compression/decompression as controlled by blosc2. This does not limit
the number of threads for the rest of the compressed_image library.

:param nthreads: Number of threads to use.
:param block_size: Optional block size override.
            )doc")
                    .def("compression", &compressed_py::dynamic_channel::compression,
                        R"doc(
:return: The compression codec used.
            )doc")
                    .def("compression_level", &compressed_py::dynamic_channel::compression_level,
                        R"doc(
:return: The compression level used.
            )doc");
    }

} // compressed_py