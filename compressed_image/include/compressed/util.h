#pragma once

#include <vector>
#include <span>

#include "macros.h"

namespace NAMESPACE_COMPRESSED_IMAGE
{

	namespace util
	{

		template <typename T>
		std::span<const T> as_const_span(std::vector<T> data)
		{
			return std::span<const T>(data.begin(), data.end());
		}

		template <typename T>
		std::span<const T> as_const_span(std::span<T> data)
		{
			return std::span<const T>(data.begin(), data.end());
		}

	}


} // NAMESPACE_COMPRESSED_IMAGE