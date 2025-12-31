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

		template <typename T>
		using compressor_var = std::variant<
			lz4_compressor<T>,
			snappy_compressor<T>,
			zstd_compressor<T>,
			deflate_compressor<T>,
			gdeflate_compressor<T>,
			cascaded_compressor<T>>;


		template <typename T>
		compressor_var<T> make_compressor(NAMESPACE_COMPRESSED_IMAGE::enums::codec codec)
		{
			switch (codec)
			{
			case NAMESPACE_COMPRESSED_IMAGE::enums::codec::lz4_gpu:
				return lz4_compressor<T>{};
			case NAMESPACE_COMPRESSED_IMAGE::enums::codec::snappy_gpu:
				return snappy_compressor<T>{};
			case NAMESPACE_COMPRESSED_IMAGE::enums::codec::zstd_gpu:
				return zstd_compressor<T>{};
			case NAMESPACE_COMPRESSED_IMAGE::enums::codec::deflate_gpu:
				return deflate_compressor<T>{};
			case NAMESPACE_COMPRESSED_IMAGE::enums::codec::gdeflate_gpu:
				return gdeflate_compressor<T>{};
			case NAMESPACE_COMPRESSED_IMAGE::enums::codec::cascaded_gpu:
				return cascaded_compressor<T>{};
			default:
				throw std::invalid_argument(
					std::format("Unknown or unsupported gpu codec: {}", static_cast<int>(codec))
				);
			}
		}

		template <typename T>
		compressor_var<T> make_compressor(const cuda::compressed_chunk<T>& chunk)
		{
			return make_compressor(chunk.context.codec);
		}


	} // namespace cuda

} // NAMESPACE_COMPRESSED_IMAGE