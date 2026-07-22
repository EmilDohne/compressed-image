#pragma once

#include <pybind11/pybind11.h>

#include "compressed/enums.h"

namespace py = pybind11;


namespace compressed_py
{

	void bind_enums(py::module_& m)
	{
		py::enum_<compressed::enums::codec>(m, "Codec", py::arithmetic(), R"pbdoc(
		Enum representing available compression codecs.

		The CPU codecs are backed by `blosc2` and the `*_gpu` codecs by `nvcomp` (CUDA).
		GPU codecs require an NVIDIA GPU and the CUDA runtime, but fall back gracefully to a
		CPU-equivalent codec when neither is available, so they are always safe to select.
		)pbdoc")
			.value("blosclz", compressed::enums::codec::blosclz, "Lightweight, fast compression optimized for high-speed decompression.")
			.value("lz4", compressed::enums::codec::lz4, "Extremely fast compression and decompression with moderate compression ratio.")
			.value("lz4hc", compressed::enums::codec::lz4hc, "High-compression variant of LZ4 with slower compression but similar fast decompression.")
			.value("zstd", compressed::enums::codec::zstd, "Zstandard compression providing high compression ratios with good speed.")
			.value("lz4_gpu", compressed::enums::codec::lz4_gpu, "(CUDA) GPU variant of lz4, higher throughput than CPU lz4. Falls back to lz4.")
			.value("snappy_gpu", compressed::enums::codec::snappy_gpu, "(CUDA) GPU snappy, fast with moderate throughput. Falls back to lz4.")
			.value("zstd_gpu", compressed::enums::codec::zstd_gpu, "(CUDA) GPU variant of zstd, higher throughput than CPU zstd. Falls back to zstd.")
			.value("deflate_gpu", compressed::enums::codec::deflate_gpu, "(CUDA) GPU variant of deflate. Falls back to zstd.")
			.value("gdeflate_gpu", compressed::enums::codec::gdeflate_gpu, "(CUDA) bit-swizzled deflate optimized for GPU. Falls back to zstd.")
			.value("cascaded_gpu", compressed::enums::codec::cascaded_gpu, "(CUDA) RLE + bitpacking + delta scheme; strong on piecewise-constant data (IDs/masks). Falls back to lz4.");
	}

} // compressed_py