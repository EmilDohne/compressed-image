#pragma once

#include "compressed/macros.h"
#include "compressed/constants.h"

#include <span>

namespace NAMESPACE_COMPRESSED_IMAGE
{
	namespace container
	{

		/// Chunked span looking into a larger image. Conceptually this is a single decompressed chunk while 
		/// iterating over an image or a channel. Retrieved by derefencing a compressed::iterator while doing e.g.
		/// 
		/// \code{.cpp}
		/// compressed::image<T> = ...;
		/// for (auto& chunk : image.channel_ref("r"))
		/// {
		///		for (auto& [index, pixel] : std::views::enumerate(chunk))
		///		{
		///			size_t x = chunk.x(index);
		///			size_t y = chunk.y(index);
		///			pixel = (x + y * image.width()) / image.size();
		///		}
		/// }
		/// \endcode
		template <typename T, size_t ChunkSize = s_default_chunksize>
		struct chunk_span : public std::ranges::view_interface<chunk_span<T, ChunkSize>>
		{
			static constexpr size_t chunk_size = ChunkSize;


			chunk_span(std::span<T> data, size_t width, size_t height, size_t chunk_index)
				: m_Data(data), m_Width(width), m_Height(height), m_ChunkIndex(chunk_index) {};

			/// For a given index into this span get the X coordinate relative to the whole image/channel.
			///
			/// \param _index The index into this `chunk_span`.
			/// 
			/// \returns The image y-coordinate for the given index.
			size_t x(size_t _index) const noexcept
			{
				const auto global_index = get_global_index(_index);
				return global_index % m_Width;
			}

			/// For a given index into this span get the y coordinate relative to the whole image/channel.
			///
			/// \param _index The index into this `chunk_span`.
			/// 
			/// \returns The image y-coordinate for the given index.
			size_t y(size_t _index) const noexcept
			{
				const auto global_index = get_global_index(_index);
				return global_index / m_Width;
			}

			/// Begin iterator over the locally held span, required to fulfill the requirements of 
			/// std::ranges::view_interface
			auto begin() noexcept { return m_Data.begin(); }

			/// end iterator over the locally held span, required to fulfill the requirements of 
			/// std::ranges::view_interface
			auto end() noexcept { return m_Data.end(); }

		private:
			std::span<T> m_Data{};
			size_t m_Width = 1;
			size_t m_Height = 1;
			size_t m_ChunkIndex = 0;

			/// For a span-local index, retrieve the global index into the larger image array (even if it is not accessible)
			/// to e.g. compute the x and y coordinates for a given offset. For the case where this is the last chunk which may
			/// not be fully populated this will still work as all previous spans are aligned to that.
			size_t get_global_index(size_t _index)
			{
				const size_t base_offset = m_ChunkIndex * chunk_size;
				return base_offset + _index;
			}
		};

	} // namespace container


} // NAMESPACE_COMPRESSED_IMAGE