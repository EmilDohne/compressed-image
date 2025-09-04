#pragma once

#include "compressed/macros.h"

#include "compressed/cuda/compressors/base.h"

namespace NAMESPACE_COMPRESSED_IMAGE
{

	namespace cuda
	{

		template <typename T>
		struct lz4_compressor final : public detail::compressor<T>
		{


			size_t max_block_size() const noexcept
			{
				return nvcompLZ4CompressionMaxAllowedChunkSize;
			};

		};


	} // namespace cuda

} // namespace NAMESPACE_COMPRESSED_IMAGE