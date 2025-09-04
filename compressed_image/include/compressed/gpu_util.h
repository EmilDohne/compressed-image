#pragma once

#include "compressed/macros.h"
#include "compressed/cuda/cuda.h"

namespace NAMESPACE_COMPRESSED_IMAGE
{

	namespace gpu
	{

		bool is_available() noexcept
		{
			try
			{
				auto& inst = cuda::cuda_api::instance();
				return true;
			}
			catch (...)
			{
				return false;
			}
		}


	} // namespace gpu

} // namespace NAMESPACE_COMPRESSED_IMAGE