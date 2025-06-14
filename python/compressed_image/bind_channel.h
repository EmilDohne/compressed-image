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
            - np.float32
            - np.float64
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
            :return: A new compressed.Channel.
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
            :return: A new `compressed.Channel`.
            )doc")
            .def_static("full_like", &compressed_py::dynamic_channel::full_like,
                py::arg("other"),
                py::arg("fill_value"),
                R"doc(
            Create a new channel with the same shape and dtype as another, filled to `fill_value`.

            :param other: Another `compressed.Channel` to mimic.
            :param fill_value: The fill value for the data, may be a float or an integer
            :return: A new `compressed.Channel`.
            )doc")
            .def_static("zeros_like", &compressed_py::dynamic_channel::zeros_like,
                py::arg("other"),
                R"doc(
            Create a new channel with the same shape and dtype as another, filled with zeros.

            :param other: Another `compressed.Channel` to mimic.
            :return: A new `compressed.Channel`.
            )doc")
            .def_property_readonly("dtype", &compressed_py::dynamic_channel::dtype, R"doc(
            :return: The numpy dtype of the underlying channel.
            )doc")
            .def("shape", &compressed_py::dynamic_channel::shape,
                R"doc(
            :return: Tuple of (height, width).
            )doc")
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
            :return: Chunk size used for compression.
            )doc")
                    .def("chunk_size",
                        py::overload_cast<size_t>(&compressed_py::dynamic_channel::chunk_size, py::const_),
                        py::arg("chunk_index"),
                        R"doc(
            :param chunk_index: Index of the chunk.
            :return: Size of the specified chunk.
            )doc")
                    .def("get_chunk", &compressed_py::dynamic_channel::get_chunk,
                        py::arg("chunk_index"),
                        R"doc(
            Get the decompressed data for a chunk.

            :param chunk_index: Index of the chunk to decompress.
            :return: 1D numpy array containing decompressed data.
            )doc")
                    .def("set_chunk", &compressed_py::dynamic_channel::set_chunk,
                        py::arg("array"), py::arg("chunk_index"),
                        R"doc(
            Replace a chunk's contents with a new array.

            :param array: 1D numpy array to copy from.
            :param chunk_index: Index of the chunk to update.
            )doc")
                    .def("get_decompressed", &compressed_py::dynamic_channel::get_decompressed,
                        R"doc(
            :return: The full decompressed data as a 2D numpy array.
            )doc")
                    .def("update_nthreads", &compressed_py::dynamic_channel::update_nthreads,
                        py::arg("nthreads"), py::arg("block_size") = compressed::s_default_blocksize,
                        R"doc(
            Update the number of threads used for compression operations.

            :param nthreads: Number of threads to use.
            :param block_size: Optional block size override.
            )doc")
                    .def("compression", &compressed_py::dynamic_channel::compression,
                        R"doc(
            :return: Compression codec used.
            )doc")
                    .def("compression_level", &compressed_py::dynamic_channel::compression_level,
                        R"doc(
            :return: Compression level.
            )doc");
    }

}