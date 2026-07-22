#pragma once

#include <stdexcept>
#include <string_view>

#include "compressed/macros.h"


namespace NAMESPACE_COMPRESSED_IMAGE
{

	namespace cuda
	{

		/// \brief Exception thrown when a CUDA library cannot be loaded
		class library_not_found : public std::runtime_error
		{
		public:
			explicit library_not_found(std::string_view msg)
				: std::runtime_error(std::string(msg))
			{
			}
		};

		/// \brief Exception thrown when a CUDA function cannot be found in the library
		class symbol_not_found : public std::runtime_error
		{
		public:
			explicit symbol_not_found(std::string_view msg)
				: std::runtime_error(std::string(msg))
			{
			}
		};

	} // namespace cuda

} // namespace NAMESPACE_COMPRESSED_IMAGE