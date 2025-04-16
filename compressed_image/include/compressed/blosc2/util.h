#pragma once

#include <string>

#include "compressed/macros.h"

#include "blosc2.h"

namespace NAMESPACE_COMPRESSED_IMAGE
{

	namespace blosc2
	{

		/// Maps a BLOSC2_ERROR_* into a std::string for use in debug logging.
		std::string map_error_code(int error_code)
		{
			return std::string(print_error(error_code));
		}

	} // blosc2

} // NAMESPACE_COMPRESSED_IMAGE