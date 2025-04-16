#pragma once

#include "compressed/macros.h"
#include "compressed/constants.h"

#include <span>

namespace NAMESPACE_COMPRESSED_IMAGE
{
	namespace container
	{

		/// Represents a chunked view into a larger image, providing access to a decompressed segment of the image or channel.
		/// 
		/// This structure acts as a lightweight view into a portion of an image, particularly when working with compressed
		/// image storage. It allows efficient iteration over the decompressed chunk while providing methods to determine 
		/// the global X and Y coordinates relative to the full image.
		/// 
		/// The chunk is retrieved by dereferencing a `compressed::iterator`, and is used in scenarios like iterating over
		/// image channels and processing decompressed data in smaller segments.
		/// 
		/// ## Example Usage:
		/// 
		/// \code{.cpp}
		/// compressed::image<T> image = ...;
		/// for (auto& chunk : image.channel_ref("r"))
		/// {
		///     for (auto& [index, pixel] : std::views::enumerate(chunk))
		///     {
		///         size_t x = chunk.x(index);
		///         size_t y = chunk.y(index);
		///         pixel = (x + y * image.width()) / image.size();
		///     }
		/// }
		/// \endcode
		/// 
		/// \tparam T The data type stored in the chunk (e.g., pixel values).
		/// \tparam ChunkSize The fixed size of each chunk, defaulting to `s_default_chunksize`.
		template <typename T>
		struct chunk_span : public std::ranges::view_interface<chunk_span<T>>
		{
			using iterator = std::span<T>::iterator;
			using const_iterator = std::span<const T>::iterator; // std::span<T>::const_iterator is C++23 only

			chunk_span() = default;

			/// Constructs a `chunk_span` pointing to a segment of an image.
			///
			/// \param data The span of data representing this chunk.
			/// \param width The total width of the full image.
			/// \param height The total height of the full image.
			/// \param chunk_index The index of this chunk in the overall compressed image sequence.
			chunk_span(std::span<T> data, size_t width, size_t height, size_t chunk_index, size_t chunk_size)
				: m_Data(data), m_Width(width), m_Height(height), m_ChunkIndex(chunk_index), m_ChunkSize(chunk_size) {};

			/// Computes the X coordinate of a given index within this chunk, relative to the full image.
			///
			/// \param _index The local index within this `chunk_span`.
			/// \returns The X coordinate in the full image.
			size_t x(size_t _index) const noexcept
			{
				const auto global_index = get_global_index(_index);
				return global_index % m_Width;
			}

			/// Computes the Y coordinate of a given index within this chunk, relative to the full image.
			///
			/// \param _index The local index within this `chunk_span`.
			/// \returns The Y coordinate in the full image.
			size_t y(size_t _index) const noexcept
			{
				const auto global_index = get_global_index(_index);
				return global_index / m_Width;
			}

			/// Returns the current chunk index we are accessing
			size_t chunk_index() const noexcept
			{
				return m_ChunkIndex;
			}

			/// Returns an iterator to the beginning of the chunk's data.
			///
			/// This is required to fulfill the requirements of `std::ranges::view_interface`.
			/// \returns An iterator to the start of the chunk's span.
			auto begin() const noexcept { return m_Data.begin(); }

			/// Returns an iterator to the end of the chunk's data.
			///
			/// This is required to fulfill the requirements of `std::ranges::view_interface`.
			/// \returns An iterator to the end of the chunk's span.
			auto end() const noexcept { return m_Data.end(); }

			/// Returns the size of the chunk.
			auto size() const noexcept { return m_Data.size(); }

		private:
			std::span<T> m_Data{};  ///< The span of data representing this chunk.
			size_t m_ChunkSize = s_default_chunksize; ///< The chunk size of all but the last chunks.
			size_t m_Width = 1;     ///< The full image width.
			size_t m_Height = 1;    ///< The full image height.
			size_t m_ChunkIndex = 0;///< The index of this chunk in the image sequence.

			/// Computes the global index of a given local index within the full image.
			///
			/// This function converts a chunk-local index into an absolute index within the full image.
			/// This is useful for computing the corresponding (X, Y) coordinates.
			///
			/// \param _index The local index within this `chunk_span`.
			/// \returns The global index within the full image.
			size_t get_global_index(size_t _index) const noexcept
			{
				const size_t base_offset = m_ChunkIndex * m_ChunkSize;
				return base_offset + _index;
			}

		};

	} // namespace container


} // NAMESPACE_COMPRESSED_IMAGE