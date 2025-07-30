#pragma once

#include "macros.h"

#ifdef COMPRESSED_IMAGE_OIIO_AVAILABLE
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/half.h>
#endif

namespace NAMESPACE_COMPRESSED_IMAGE
{
	namespace enums
	{
		/// Enum representing available compression codecs.
		///
		/// These codecs are inherited from `blosc2` and define different compression algorithms
		/// that can be used when storing or transmitting compressed images.
		enum class codec
		{
			blosclz, ///< Lightweight, fast compression optimized for high-speed decompression.
			lz4,     ///< Extremely fast compression and decompression with moderate compression ratio.
			lz4hc,   ///< High-compression variant of LZ4 with slower compression but similar fast decompression.
			zstd,	 ///< Zstandard compression providing high compression ratios with decent speed.
		};


#ifdef COMPRESSED_IMAGE_OIIO_AVAILABLE

		/// Get a OpenImageIO TypeDesc based on the given template parameter returning OIIO::TypeDesc::Unknown
		/// if the image coordinate is not part of the valid template specializations for photoshop buffers
		template <typename T>
		constexpr OIIO::TypeDesc get_type_desc()
		{
			if constexpr (std::is_same_v<T, uint8_t>)
			{
				return OIIO::TypeDesc::UINT8;
			}
			else if constexpr (std::is_same_v<T, int8_t>)
			{
				return OIIO::TypeDesc::INT8;
			}
			else if constexpr (std::is_same_v<T, uint16_t>)
			{
				return OIIO::TypeDesc::UINT16;
			}
			else if constexpr (std::is_same_v<T, int16_t>)
			{
				return OIIO::TypeDesc::INT16;
			}
			else if constexpr (std::is_same_v<T, uint32_t>)
			{
				return OIIO::TypeDesc::UINT32;
			}
			else if constexpr (std::is_same_v<T, int32_t>)
			{
				return OIIO::TypeDesc::INT32;
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				return OIIO::TypeDesc::FLOAT;
			}
			else if constexpr (std::is_same_v<T, half>)
			{
				return OIIO::TypeDesc::HALF;
			}
			else
			{
				return OIIO::TypeDesc::UNKNOWN;
			}
		}

#endif // COMPRESSED_IMAGE_OIIO_AVAILABLE

	} // namespace enums

} // NAMESPACE_COMPRESSED_IMAGE
