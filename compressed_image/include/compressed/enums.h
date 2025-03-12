#pragma once

#include "Macros.h"

#ifdef COMPRESSED_IMAGE_OIIO_AVAILABLE
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/half.h>
#endif

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
			else if constexpr (std::is_same_v<T, uint16_t>)
			{
				return OIIO::TypeDesc::UINT16;
			}
			else if constexpr (std::is_same_v<T, uint32_t>)
			{
				return OIIO::TypeDesc::UINT32;
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				return OIIO::TypeDesc::FLOAT;
			}
			else if constexpr (std::is_same_v<T, half>)
			{
				return OIIO::TypeDesc::HALF;
			}
			return OIIO::TypeDesc::UNKNOWN;
		}

#endif

	} // namespace enums

} // NAMESPACE_COMPRESSED_IMAGE
