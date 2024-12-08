#pragma once

#include "Macros.h"

namespace NAMESPACE_COMPRESSED_IMAGE
{
	namespace enums
	{

		/// Memory order of the image where `planar` is:
		/// 
		/// RRRR... GGGG... BBBB... AAAA...
		/// 
		/// and `interleaved` is 
		/// 
		/// RGBA RGBA RGBA RGBA...
		/// 
		/// When iterating a single channel at a time storing the images in a planar fashion might lead to faster results
		/// while for iterating multiple channels at a time it will make more sense to store the images interleaved.
		enum class memory_order
		{
			planar,
			interleaved
		};


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
