#pragma once

#include "macros.h"
#include "blosc2_wrapper.h"


namespace NAMESPACE_COMPRESSED_IMAGE
{

	/// Iterator operating in strides over a std::span<T> starting at the default 
	template <typename T>
	struct strided_iterator
	{
		// Iterator type definitions
		using iterator_category = std::forward_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = T;
		using pointer = value_type*;
		using reference = value_type&;

		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		strided_iterator(std::span<T> data, std::size_t start_offset, std::int64_t stride)
			: m_Data(data), m_DataIndex(start_offset), m_StartOffset(start_offset), m_Stride(stride)
		{
			if (start_offset > data.size())
			{
				throw std::out_of_range("Start offset would exceed size of passed buffer")
			}
		}

		// Dereference operator: decompress the current chunk and recompress (if necessary) the last channel.
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		value_type operator*()
		{
			return m_Data[m_DataIndex];
		}

		// Pre-increment operator: move to the next chunk
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		strided_iterator& operator++()
		{
			m_DataIndex += m_Stride;
			return *this;
		}

		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		bool operator==(const strided_iterator& other) const noexcept
		{
			return m_DataIndex == other.m_DataIndex;
		}

		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		bool operator!=(const strided_iterator& other) const noexcept
		{
			return !(*this == other);
		}

	private:
		std::span<T> m_Data;
		std::size_t m_DataIndex = 0;
		const std::size_t m_StartOffset = 0;
		const std::int64_t m_Stride = 1;
	};

} // NAMESPACE_COMPRESSED_IMAGE