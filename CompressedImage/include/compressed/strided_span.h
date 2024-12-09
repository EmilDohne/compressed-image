#pragma once

#include "macros.h"
#include "iterator.h"

#include <span>

namespace NAMESPACE_COMPRESSED_IMAGE
{

	/// A non-owning view over contiguous memory iterating in strides rather than 
	template <typename T>
	struct strided_span
	{
		using data_type = std::span<T>;
		using value_type = T;
		using iterator = strided_iterator;

		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		strided_span(data_type data, std::size_t start_offset, std::size_t stride)
			: m_Data(data), m_StartOffset(start_offset), m_Stride(stride)
		{
			if (m_Stride == 0)
			{
				throw std::invalid_argument("Stride cannot be zero");
			}
			if (m_StartOffset > m_Data.size())
			{
				throw std::out_of_range("Unable to construct StridedSpan with start_offset exceeding data size");
			}
			if (m_Stride > m_Data.size())
			{
				throw std::out_of_range("Unable to construct StridedSpan with stride exceeding data size");
			}
		}

		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		T& operator[](std::size_t index)
		{
			const std::size_t actual_index = index * m_Stride;
			return m_Data[m_StartOffset + actual_index];
		}

		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		const T& operator[](std::size_t index) const
		{
			const std::size_t actual_index = index * m_Stride;
			return m_Data[m_StartOffset + actual_index];
		}


		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		auto begin() const
		{
			return strided_iterator<T>(m_Data, m_StartOffset, m_Stride);
		}

		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		auto end() const
		{
			return strided_iterator<T>(m_Data, (last_valid_idx() + 1) * m_Stride, m_Stride);
		}


		/// Get the total amount of items stored in the strided span
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		std::size_t size() const
		{
			// Get the element 1 past the last indexable index
			return last_valid_idx() + 1;
		}

		/// Get the actual amount of items stored in the underlying data.
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		std::size_t actual_size() const
		{
			return m_Data.size();
		}


		/// Get the total byte size of the span.
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		std::size_t byte_size() const
		{
			return m_Data.size() * sizeof(T);
		}


		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		std::size_t last_valid_idx() const
		{
			return m_Data.size() / m_Stride;
		}

	private:
		data_type m_Data;
		std::size_t m_StartOffset = 1;
		std::size_t m_Stride = 1;
	};


} // NAMESPACE_COMPRESSED_IMAGE