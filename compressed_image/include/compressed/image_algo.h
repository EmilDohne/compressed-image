#pragma once

#include "macros.h"
#include "util.h"
#include "detail/scoped_timer.h"

#include <algorithm>
#include <span>
#include <vector>
#include <execution>

namespace NAMESPACE_COMPRESSED_IMAGE
{

	namespace image_algo
	{

		/// Interleave the spans into a unified buffer taking any number of input spans (could be channels).
		///
		/// could be called like this for example:
		/// 
		/// \code{.cpp}
		/// std::span<T> interleaved;
		/// std::vector<std::span<T>> channels(channel_r, channel_g, channel_b, channel_a);
		/// 
		/// Render::interleave(interleaved, channels);
		/// \endcode
		/// 
		/// \tparam T The data type to interleave
		/// 
		/// \param buffer The preallocated buffer to interleave into. Must be exactly the size of spans[0].size * spans.size()
		/// \param spans  The input spans to interleave. Must all be the same size
		template <typename T>
		void interleave(std::span<T> buffer, const std::vector<std::span<const T>>& spans)
		{
			_COMPRESSED_PROFILE_FUNCTION();
			if (spans.empty())
			{
				throw std::invalid_argument("Interleave: No spans provided for interleaving.");
			}

			for (const auto& span : spans)
			{
				if (span.size() != spans.front().size())
				{
					throw std::invalid_argument("Interleave: All input spans must have the same size.");
				}
			}

			if (buffer.size() != spans.front().size() * spans.size())
			{
				throw std::invalid_argument("Interleave: Provided buffer is not large enough to hold all the elements to interleave.");
			}

			auto indices = std::views::iota(static_cast<std::size_t>(0), spans.front().size());
			std::for_each(std::execution::par_unseq, indices.begin(), indices.end(), [&buffer, &spans](auto idx)
				{
					const std::size_t start_idx = spans.size() * idx;
					for (size_t i = 0; i < spans.size(); ++i)
					{
						buffer[start_idx + i] = spans[i][idx];
					}
				});
		}


		/// Deinterleave a unified buffer into multiple channels, using the preallocated buffers.
		///
		/// Could be called like this, for example:
		///
		/// \code{.cpp}
		///
		/// std::vector<T> interleaved = ...; // Previously interleaved data
		/// std::vector<std::span<T>> channels = ...; // The channels to deinterleave into
		/// auto deinterleaved = Render::deinterleave(interleaved, channels);
		/// 
		/// \endcode
		/// 
		/// \tparam T The data type to deinterleave.
		///
		/// \param interleaved The input buffer containing the interleaved data.
		/// \param channel_spans The spans to deinterleave into.
		template <typename T>
		void deinterleave(std::span<const T> interleaved, std::vector<std::span<T>>& channel_spans)
		{
			_COMPRESSED_PROFILE_FUNCTION();
			// Ensure all spans have the same size
			std::size_t size = channel_spans.front().size();
			if (!std::all_of(channel_spans.begin(), channel_spans.end(), [size](const std::span<T>& span) { return span.size() == size; }))
			{
				throw std::invalid_argument("Deinterleave: All output spans must have the same size.");
			}

			// Ensure the interleaved buffer size matches the total size of the spans
			if (interleaved.size() != size * channel_spans.size())
			{
				throw std::invalid_argument("Deinterleave: Input buffer size does not match the expected size for deinterleaving.");
			}

			// Perform deinterleaving
			const std::size_t spans_size = channel_spans.size();
			auto indices = std::views::iota(static_cast<std::size_t>(0), channel_spans.front().size());
			std::for_each(std::execution::par_unseq, indices.begin(), indices.end(), [&](auto idx)
				{
					const size_t interleaved_base_idx = idx * spans_size;
					for (size_t i = 0; i < spans_size; ++i)
					{
						channel_spans[i][idx] = interleaved[interleaved_base_idx + i];
					}
				});
		}


		/// Deinterleave a unified buffer into multiple channels, using the preallocated buffers.
		///
		/// Could be called like this, for example:
		///
		/// \code{.cpp}
		///
		/// std::vector<T> interleaved = ...; // Previously interleaved data
		/// std::vector<std::vector<T>> channels = ...; // The channels to deinterleave into
		/// auto deinterleaved = Render::deinterleave(interleaved, channels);
		/// 
		/// \endcode
		/// 
		/// \tparam T The data type to deinterleave.
		///
		/// \param interleaved The input buffer containing the interleaved data.
		/// \param channel_vecs The vecs to deinterleave into.
		template <typename T>
		void deinterleave(std::span<const T> interleaved, std::vector<std::vector<T>>& channel_vecs)
		{
			std::vector<std::span<T>> spans{};
			for (auto& vec : channel_vecs)
			{
				spans.push_back(std::span<T>(vec.begin(), vec.end()));
			}
			deinterleave<T>(interleaved, spans);
		}

		/// Deinterleave a unified buffer into multiple channels, using the preallocated buffers.
		///
		/// Could be called like this, for example:
		///
		/// \code{.cpp}
		///
		/// std::vector<T> interleaved = ...; // Previously interleaved data
		/// std::vector<std::vector<T>> channels = ...; // The channels to deinterleave into
		/// auto deinterleaved = Render::deinterleave(interleaved, channels);
		/// 
		/// \endcode
		/// 
		/// \tparam T The data type to deinterleave.
		///
		/// \param interleaved The input buffer containing the interleaved data.
		/// \param channel_vecs The vecs to deinterleave into.
		template <typename T>
		void deinterleave(std::span<const T> interleaved, std::vector<util::default_init_vector<T>>& channel_vecs)
		{
			std::vector<std::span<T>> spans{};
			for (auto& vec : channel_vecs)
			{
				spans.push_back(std::span<T>(vec.begin(), vec.end()));
			}
			deinterleave<T>(interleaved, spans);
		}
	
	} // image_algo

} // NAMESPACE_COMPRESSED_IMAGE