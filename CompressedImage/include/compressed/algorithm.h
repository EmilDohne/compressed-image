#pragma once

#include <execution>
#include <thread>

#include "macros.h"
#include "image.h"

namespace NAMESPACE_COMPRESSED_IMAGE
{

	namespace impl
	{
		template <class ExecutionPolicy>
		static void update_image_threads(ExecutionPolicy& policy, image<T>& _image)
		{
			if constexpr (std::is_same_v<ExecutionPolicy, std::execution::par> || std::is_same_v<ExecutionPolicy, std::execution::par_unseq>)
			{
				_image.update_nthreads(std::thread::hardware_concurrency());
			}
			else
			{
				_image.update_nthreads(1);
			}
		}
	}


	/// Iterate over all elements of the image for all the channels applying function f.
	///
	/// \param policy The execution policy to apply to the iteration
	/// \param image  The image to operate over
	/// \param func   The function to apply to the elements. Should take a singular param T
	// ---------------------------------------------------------------------------------------------------------------------
	// ---------------------------------------------------------------------------------------------------------------------
	template<typename T, class ExecutionPolicy, class UnaryFunc>
	constexpr UnaryFunc for_each(ExecutionPolicy&& policy, image<T>& _image, UnaryFunc func)
	{
		update_image_threads(policy, _image);

		/// Iterate over all chunks applying the function to them
		for (auto it = _image.begin(); it != _image.end(); ++it)
		{
			auto& data = *it;
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
	constexpr UnaryFunc for_each(ExecutionPolicy&& policy, image<T>& _image, std::string_view channelname, UnaryFunc func)
	{
		update_image_threads(policy, _image);

		/// Iterate over all chunks applying the function to them
		for (strided_span<T>& decompressed_chunk : _image)
		{
			decompressed_chunk.set_channel
			std::for_each(policy, decompressed_chunk.begin(), decompressed_chunk.end(), func);
		}
		return func;
	}


} // NAMESPACE_COMPRESSED_IMAGE