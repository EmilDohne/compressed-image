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

		These codecs are inherited from `blosc2` and define different compression algorithms
		that can be used when storing or transmitting compressed images.
		)pbdoc")
			.value("blosclz", compressed::enums::codec::blosclz, "Lightweight, fast compression optimized for high-speed decompression.")
			.value("lz4", compressed::enums::codec::lz4, "Extremely fast compression and decompression with moderate compression ratio.")
			.value("lz4hc", compressed::enums::codec::lz4hc, "High-compression variant of LZ4 with slower compression but similar fast decompression.")
			.value("zstd", compressed::enums::codec::zstd, "Zstandard compression providing high compression ratios with good speed.");
	}

} // compressed_py