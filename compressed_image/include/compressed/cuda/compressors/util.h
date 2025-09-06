#pragma once

#include "compressed/macros.h"

#include <nvcomp.h>
#ifdef COMPRESSED_IMAGE_OIIO_AVAILABLE
#include <OpenImageIO/imageio.h>
#endif

namespace NAMESPACE_COMPRESSED_IMAGE
{

	namespace cuda
	{

		namespace util
		{

			template<typename T>
			constexpr nvcompType_t to_nvcomp_type()
			{
				if constexpr (std::is_same_v<T, char> || std::is_same_v<T, int8_t>)
					return NVCOMP_TYPE_CHAR;
				else if constexpr (std::is_same_v<T, unsigned char> || std::is_same_v<T, uint8_t>)
					return NVCOMP_TYPE_UCHAR;
				else if constexpr (std::is_same_v<T, short> || std::is_same_v<T, int16_t>)
					return NVCOMP_TYPE_SHORT;
				else if constexpr (std::is_same_v<T, unsigned short> || std::is_same_v<T, uint16_t>)
					return NVCOMP_TYPE_USHORT;
				else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, int32_t>)
					return NVCOMP_TYPE_INT;
				else if constexpr (std::is_same_v<T, unsigned int> || std::is_same_v<T, uint32_t>)
					return NVCOMP_TYPE_UINT;
				else if constexpr (std::is_same_v<T, long long> || std::is_same_v<T, int64_t>)
					return NVCOMP_TYPE_LONGLONG;
				else if constexpr (std::is_same_v<T, unsigned long long> || std::is_same_v<T, uint64_t>)
					return NVCOMP_TYPE_ULONGLONG;
#ifdef COMPRESSED_IMAGE_OIIO_AVAILABLE
				else if constexpr (std::is_same_v<T, Imath::half>)
					return NVCOMP_TYPE_FLOAT16;
#endif
				else if constexpr (std::is_same_v<T, float>)
					return NVCOMP_TYPE_UINT; // fallback: map float -> uint
				else if constexpr (std::is_same_v<T, double>)
					return NVCOMP_TYPE_ULONGLONG; // fallback: map double -> ulonglong
				else
					return NVCOMP_TYPE_BITS; // fallback default
			}

		} // namespace util

	} // namespace cuda

} // namespace NAMESPACE_COMPRESSED_IMAGE