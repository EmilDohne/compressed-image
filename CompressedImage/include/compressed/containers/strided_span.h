#pragma once

#include "macros.h"
#include "iterators/strided_iterator.h"
#include "iterators/group_iterator.h"

#include <span>
#include <cassert>

namespace NAMESPACE_COMPRESSED_IMAGE
{

	/// A non-owning view over contiguous memory iterating in strides rather than 
	template <typename T>
	struct strided_span
	{
		using data_type = std::span<T>;
		using value_type = T;
		using iterator = strided_iterator<T>;

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
		void update_stride(std::size_t start, std::size_t stride)
		{
			m_StartOffset = start;
			m_Stride = stride;

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
		iterator begin() const
		{
			return strided_iterator<T>(m_Data, m_StartOffset, m_Stride);
		}

		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		iterator end() const
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

	/// Aggregate type combining multiple strided_spans asserting that the spans do not 
	/// overlap in order to allow the compiler to make better 
	template <typename T, std::size_t N>
	struct strided_span_group
	{
		using span_type = strided_span<T>;

		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		strided_span_group(const std::array<strided_span<T>, N>& input_spans)
			: m_Spans(input_spans)
		{
			ensure_non_aliasing();
		}

		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		std::size_t size() const
		{
			return m_Spans[0].size();
		}

		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		span_type& operator[](std::size_t index)
		{
			return m_Spans[index];
		}

		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		const span_type& operator[](std::size_t index) const
		{
			return m_Spans[index];
		}


		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		constexpr auto begin() const 
		{ 
			return group_iterator{ *this, 0 };
		}

		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		constexpr auto end() const 
		{ 
			return group_iterator{ *this, size() };
		}

	private:

		std::array<span_type, N> m_Spans;


	private:
		// ---------------------------------------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------------------------------------------
		void ensure_non_aliasing()
		{
			const auto first_size = m_Spans[0].size();

			for (std::size_t i = 0; i < N; ++i)
			{
				assert(m_Spans[i].size() == first_size && "All spans must have the same size");
				for (std::size_t j = i + 1; j < N; ++j)
				{
					assert(&m_Spans[i][0] != &m_Spans[j][0] && "Channels must not alias each other");
				}
			}
		}
	};


} // NAMESPACE_COMPRESSED_IMAGE