#pragma once

#include <execution>
#include <thread>

#include "macros.h"
#include "image.h"

namespace NAMESPACE_COMPRESSED_IMAGE
{

	/// Iterate over all elements of the image for all the channels applying function f.
	///
	/// \param policy The execution policy to apply to the iteration
	/// \param image  The image to operate over
	/// \param func   The function to apply to the elements. Should take a singular param T
	// ---------------------------------------------------------------------------------------------------------------------
	// ---------------------------------------------------------------------------------------------------------------------
	template<typename T, class ExecutionPolicy, class UnaryFunc>
	constexpr UnaryFunc for_each(ExecutionPolicy&& policy, image<T> image, UnaryFunc func)
	{
		if constexpr (std::is_same_v<ExecutionPolicy, std::execution::par> || std::is_same_v<ExecutionPolicy, std::execution::par_unseq>)
		{
			image.update_nthreads(std::thread::hardware_concurrency());
		}
		else
		{
			image.update_nthreads(1);
		}

		image<T>::iterator first = image.begin();
		image<T>::iterator last = image.end();

		/// Iterate over all chunks applying the function to them
		for (; first != last; ++first)
		{
			auto& data = *first;
			std::for_each(policy, data.begin(), data.end(), func);
		}

		return func;
	}

	/// Iterate over all elements of the image for all the channels applying function f.
	///
	/// \param policy The execution policy to apply to the iteration
	/// \param image  The image to operate over
	/// \param func   The function to apply to the elements. Should take a singular param T
	// ---------------------------------------------------------------------------------------------------------------------
	// ---------------------------------------------------------------------------------------------------------------------
	template<typename T, class ExecutionPolicy, class UnaryFunc>
	constexpr UnaryFunc for_each_channel(ExecutionPolicy&& policy, image<T> image, std::string channalename, UnaryFunc func)
	{
		if constexpr (std::is_same_v<ExecutionPolicy, std::execution::par> || std::is_same_v<ExecutionPolicy, std::execution::par_unseq>)
		{
			image.update_nthreads(std::thread::hardware_concurrency());
		}
		else
		{
			image.update_nthreads(1);
		}


	}

} // NAMESPACE_COMPRESSED_IMAGE