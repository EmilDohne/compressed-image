#pragma once

#include "Macros.h"

namespace NAMESPACE_COMPRESSED_IMAGE
{
	namespace enums
	{
		/// The compression codecs known to us. Inherited from blosc2.
		enum class codec
		{
			blosclz,
			lz4,
			lz4hc,
			zlib,
			zstd
		};

	} // namespace enums

} // NAMESPACE_COMPRESSED_IMAGE
