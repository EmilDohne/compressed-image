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

        )doc")
            .def(py::init<
                    std::vector<py::array>,
                    size_t,
                    size_t,
                    std::vector<std::string>,
                    compressed::enums::codec,
                    size_t,
                    size_t,
                    size_t>(),
                py::arg("channels"),
                py::arg("width"),
                py::arg("height"),
                py::arg("channel_names") = std::vector<std::string>{},
                py::arg("compression_codec") = compressed::enums::codec::lz4,
                py::arg("compression_level") = 9,
                py::arg("block_size") = compressed::s_default_blocksize,
                py::arg("chunk_size") = compressed::s_default_chunksize,
                R"doc(
                Construct a dynamic compressed image from a list of numpy arrays.

                :param channels: List of 2D numpy arrays representing image channels.
                :param width: Width of the image.
                :param height: Height of the image.
                :param channel_names: Optional list of channel names.
                :param compression_codec: Compression codec.
                :param compression_level: Compression level.
                :param block_size: Block size.
                :param chunk_size: Chunk size.
            )doc")
            .def_static("read",
                py::overload_cast<
                    std::filesystem::path,
                    int,
                    compressed::enums::codec,
                    size_t,
                    size_t,
                    size_t>(&compressed_py::dynamic_image::read),
                py::arg("filepath"), py::arg("subimage"),
                py::arg("compression_codec") = compressed::enums::codec::lz4,
                py::arg("compression_level") = 9,
                py::arg("block_size") = compressed::s_default_blocksize,
                py::arg("chunk_size") = compressed::s_default_chunksize,
                R"doc(
                Read an entire image from disk.
            )doc")

            .def_static("read",
                py::overload_cast<
                    std::filesystem::path,
                    int,
                    std::vector<int>,
                    compressed::enums::codec,
                    size_t,
                    size_t,
                    size_t>(&compressed_py::dynamic_image::read),
                py::arg("filepath"), py::arg("subimage"), py::arg("channel_indices"),
                py::arg("compression_codec") = compressed::enums::codec::lz4,
                py::arg("compression_level") = 9,
                py::arg("block_size") = compressed::s_default_blocksize,
                py::arg("chunk_size") = compressed::s_default_chunksize,
                R"doc(
                Read an image from disk selecting specific channel indices.
            )doc")

            .def_static("read",
                py::overload_cast<
                    std::filesystem::path,
                    int,
                    std::vector<std::string>,
                    compressed::enums::codec,
                    size_t,
                    size_t,
                    size_t>(&compressed_py::dynamic_image::read),
                py::arg("filepath"), py::arg("subimage"), py::arg("channel_names"),
                py::arg("compression_codec") = compressed::enums::codec::lz4,
                py::arg("compression_level") = 9,
                py::arg("block_size") = compressed::s_default_blocksize,
                py::arg("chunk_size") = compressed::s_default_chunksize,
                R"doc(
                Read an image from disk selecting specific channel names.
            )doc")

            .def("add_channel", &compressed_py::dynamic_image::add_channel,
                py::arg("data"),
                py::arg("width"),
                py::arg("height"),
                py::arg("name") = std::nullopt,
                py::arg("compression_codec") = compressed::enums::codec::lz4,
                py::arg("compression_level") = 5,
                R"doc(
                Add a channel to the image.
            )doc")

            .def("channel", py::overload_cast<size_t>(&compressed_py::dynamic_image::channel),
                py::arg("index"),
                R"doc(
                Get channel by index.
            )doc")

            .def("channel", py::overload_cast<const std::string_view>(&compressed_py::dynamic_image::channel),
                py::arg("name"),
                R"doc(
                Get channel by name.
            )doc")

            .def("channels", &compressed_py::dynamic_image::channels,
                R"doc(
                Get list of channels as dynamic objects.
            )doc")

            .def("get_decompressed", &compressed_py::dynamic_image::get_decompressed,
                R"doc(
                Get all channels as decompressed numpy arrays.
            )doc")

            .def("get_channel_index", &compressed_py::dynamic_image::get_channel_index,
                py::arg("channelname"),
                R"doc(
                Return index of a channel by name.
            )doc")

            .def("print_statistics", &compressed_py::dynamic_image::print_statistics,
                R"doc(
                Print image compression statistics.
            )doc")

            .def("compression_ratio", &compressed_py::dynamic_image::compression_ratio,
                R"doc(
                :return: Compression ratio.
            )doc")

            .def("width", &compressed_py::dynamic_image::width,
                R"doc(
                :return: Image width.
            )doc")

            .def("height", &compressed_py::dynamic_image::height,
                R"doc(
                :return: Image height.
            )doc")

            .def("num_channels", &compressed_py::dynamic_image::num_channels,
                R"doc(
                :return: Number of channels.
            )doc")

            .def("update_nthreads", &compressed_py::dynamic_image::update_nthreads,
                py::arg("nthreads"),
                R"doc(
                Update the number of threads used.
            )doc")

            .def("chunk_size", &compressed_py::dynamic_image::chunk_size,
                R"doc(
                :return: Chunk size.
            )doc")

            .def("metadata", py::overload_cast<const json_ordered&>(&compressed_py::dynamic_image::metadata),
                py::arg("metadata"),
                R"doc(
                Set metadata.
            )doc")

            .def("metadata", py::overload_cast<>(&compressed_py::dynamic_image::metadata),
                R"doc(
                Get metadata.
            )doc");
    }

} compressed_py