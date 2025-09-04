#pragma once

#include "compressed/macros.h"


namespace NAMESPACE_COMPRESSED_IMAGE
{

	namespace cuda
	{

		namespace enums
		{

			/// \brief the storage location of a given compressing data buffer.
			enum class storage_location
			{
				device, ///< Data is stored on the device (gpu) and only pulled back to the cpu when accessing
				host	///< Data is stored on the host (cpu) incurring an additional cost for copying back and forth memory.
			};

		} // namespace enums

	} // namespace cuda

} // namespace NAMESPACE_COMPRESSED_IMAGE