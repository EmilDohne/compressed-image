#pragma once

#include <execution>
#include <thread>

#include "macros.h"
#include "image.h"

namespace NAMESPACE_COMPRESSED_IMAGE
{

	template<typename T, class ExecutionPolicy, class UnaryFunc>
	constexpr UnaryFunc for_each(ExecutionPolicy&& policy, Image<T> image, UnaryFunc f)
	{
		auto* compression_context = image.compression_context();
		auto* decompression_context = image.decompression_context();

		if constexpr (std::is_same_v<ExecutionPolicy, std::execution::par> || std::is_same_v<ExecutionPolicy, std::execution::par_unseq>)
		{
			image.update_nthreads(std::thread::hardware_concurrency());
		}
		else
		{
			image.update_nthreads(1);
		}

		Image<T>::Iterator first = image.begin();
		Image<T>::Iterator last = image.end();

		for (; first != last; ++first)
		{
			std::span<T> data = *first;
			std::for_each(policy, data.begin(), data.end(), f);
		}

		return f;
	}



} // NAMESPACE_COMPRESSED_IMAGE