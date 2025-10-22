#pragma once

#include "compressed/enums.h"

#include "compressed/cuda/compressors/lz4.h"
#include "compressed/cuda/compressors/snappy.h"
#include "compressed/cuda/compressors/zstd.h"
#include "compressed/cuda/compressors/deflate.h"
#include "compressed/cuda/compressors/gdeflate.h"
#include "compressed/cuda/compressors/cascaded.h"

namespace NAMESPACE_COMPRESSED_IMAGE
{

	namespace cuda
	{

		using compressor_var = std::variant<
			lz4_compressor,
			snappy_compressor,
			zstd_compressor,
			deflate_compressor,
			gdeflate_compressor,
			cascaded_compressor>;


		inline compressor_var make_compressor(NAMESPACE_COMPRESSED_IMAGE::enums::codec codec)
		{
			switch (codec)
			{
			case NAMESPACE_COMPRESSED_IMAGE::enums::codec::lz4_gpu:
				return lz4_compressor{};
			case NAMESPACE_COMPRESSED_IMAGE::enums::codec::snappy_gpu:
				return snappy_compressor{};
			case NAMESPACE_COMPRESSED_IMAGE::enums::codec::zstd_gpu:
				return zstd_compressor{};
			case NAMESPACE_COMPRESSED_IMAGE::enums::codec::deflate_gpu:
				return deflate_compressor{};
			case NAMESPACE_COMPRESSED_IMAGE::enums::codec::gdeflate_gpu:
				return gdeflate_compressor{};
			case NAMESPACE_COMPRESSED_IMAGE::enums::codec::cascaded_gpu:
				return cascaded_compressor{};
			default:
				throw std::invalid_argument(
					std::format("Unknown or unsupported gpu codec: {}", static_cast<int>(codec))
				);
			}
		}


	} // namespace cuda

} // NAMESPACE_COMPRESSED_IMAGE