#pragma once

#include "macros.h"
#include "compressed/containers/strided_span.h"
#include "compressed/iterators/strided_iterator.h"
#include "compressed/iterators/group_iterator.h"

#include <span>
#include <cassert>

namespace NAMESPACE_COMPRESSED_IMAGE
{

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