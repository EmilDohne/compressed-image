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


			/// \brief Convert a nvcompStatus_t object into a human-readable string for printing.
			/// \param status The status to convert 
			/// \return A human-readable string explaining the error.
			constexpr inline std::string_view status_t_to_string(const nvcompStatus_t& status) noexcept
			{
				switch (status)
				{
				case nvcompStatus_t::nvcompSuccess:
					return "success";
				case nvcompStatus_t::nvcompErrorInvalidValue:
					return "invalid value";
				case nvcompStatus_t::nvcompErrorNotSupported:
					return "not supported";
				case nvcompStatus_t::nvcompErrorCannotDecompress:
					return "cannot decompress";
				case nvcompStatus_t::nvcompErrorBadChecksum:
					return "bad checksum";
				case nvcompStatus_t::nvcompErrorCannotVerifyChecksums:
					return "cannot verify checksums";
				case nvcompStatus_t::nvcompErrorOutputBufferTooSmall:
					return "output buffer too small";
				case nvcompStatus_t::nvcompErrorWrongHeaderLength:
					return "wrong header length";
				case nvcompStatus_t::nvcompErrorAlignment:
					return "alignment error";
				case nvcompStatus_t::nvcompErrorChunkSizeTooLarge:
					return "chunk size too large";
				case nvcompStatus_t::nvcompErrorCannotCompress:
					return "cannot compress";
				case nvcompStatus_t::nvcompErrorWrongInputLength:
					return "wrong input length";
				case nvcompStatus_t::nvcompErrorCudaError:
					return "CUDA error";
				case nvcompStatus_t::nvcompErrorInternal:
					return "internal error";
				default:
					return "unknown error";
				}
			}

		} // namespace util

	} // namespace cuda

} // namespace NAMESPACE_COMPRESSED_IMAGE