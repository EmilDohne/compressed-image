#pragma once

#include "Macros.h"

#include <format>

namespace NAMESPACE_COMPRESSED_IMAGE
{

	namespace impl
	{

		/// View over a compression context with the full sized data
		/// as well as the fitted data.
		template <typename T>
		struct compression_view
		{

			compression_view() = default;
			compression_view(std::span<T> buffer)
			{
				this->data = buffer;
				this->fitted_data = buffer;
			}

			std::size_t max_byte_size() const noexcept { return this->data.size() * sizeof(T); }
			std::size_t max_size() const noexcept { return this->data.size(); }

			std::size_t byte_size() const noexcept { return this->fitted_data.size() * sizeof(T); }
			std::size_t size() const noexcept { return this->fitted_data.size(); }

			bool was_refitted() const noexcept { return this->m_WasRefitted; }

			void refit(std::size_t size)
			{
				if (size > this->data.size())
				{
					throw std::invalid_argument(std::format("Invalid size argument provided, can at most refit to {}", data.size()));
				}
				this->fitted_data = std::span<T>(this->data.data(), size);
				this->m_WasRefitted = true;
			}

			std::span<T> data;
			std::span<T> fitted_data;

		private:
			bool m_WasRefitted = false;
		};


	} // namespace impl

} // NAMESPACE_COMPRESSED_IMAGE
