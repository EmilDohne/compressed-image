#pragma once

#include <vector>
#include <span>
#include <iostream>

#include "macros.h"

namespace NAMESPACE_COMPRESSED_IMAGE
{

	namespace util
	{

		inline uint8_t ensure_compression_level(size_t compression_level)
		{
			if (compression_level > 9)
			{
				std::cout << "Blosc2 only supports compression levels from 0-9, truncating value to this" << std::endl;
				compression_level = 9;
			}
			return static_cast<uint8_t>(compression_level);
		}

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