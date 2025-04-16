#pragma once

#include <vector>
#include <span>
#include <iostream>

#include "macros.h"

namespace NAMESPACE_COMPRESSED_IMAGE
{

	namespace util
	{

		namespace detail
		{
			/// Allocator adaptor that interposes construct() calls to
			/// convert value initialization into default initialization.
			/// taken from
			/// https://stackoverflow.com/questions/21028299/is-this-behavior-of-vectorresizesize-type-n-under-c11-and-boost-container/21028912#21028912
			template <typename T, typename A = std::allocator<T>>
			class default_init_allocator : public A 
			{
				typedef std::allocator_traits<A> a_t;
			public:
				template <typename U> struct rebind {
					using other =
						default_init_allocator<
						U, typename a_t::template rebind_alloc<U>
						>;
				};

				using A::A;

				template <typename U>
				void construct(U* ptr)
					noexcept(std::is_nothrow_default_constructible<U>::value) {
					::new(static_cast<void*>(ptr)) U;
				}
				template <typename U, typename...Args>
				void construct(U* ptr, Args&&... args) {
					a_t::construct(static_cast<A&>(*this),
						ptr, std::forward<Args>(args)...);
				}
			};
		}

		/// std::vector that bypasses explicitly setting T to 0 on vector.resize().
		/// should be used when we know we will explicitly fill the memory after.
		/// This is significantly faster than a regular std::vector scaling with size
		/// i.e.for 4kb this would be about 2x as fast, while for even just 65kb it's 
		/// 45x faster. It however cannot be used interchangeably with vector as it is a 
		/// different allocator than regular std::allocator and should primarily be used
		/// for e.g. swap buffers or buffers where you then get a span from.
		template <typename T>
		using default_init_vector = std::vector<T, detail::default_init_allocator<T>>;

		/// Ensure that the passed chunk size is cleanly divisible by T.
		/// Returns true if this is the case, false if it is not.
		template <typename T>
		constexpr bool ensure_chunk_size(size_t chunk_size) noexcept
		{
			return chunk_size % sizeof(T) == 0;
		}

		/// Valiate that the passed chunk size is cleanly divisible by T.
		/// Throws a std::invalid_argument if this is not the case. 
		/// 
		/// \param chunk_size The chunk size to check against.
		/// \param context The context to print along with it
		template <typename T>
		void validate_chunk_size(size_t chunk_size, std::string_view context)
		{
			if (!util::ensure_chunk_size<T>(chunk_size))
			{
				throw std::invalid_argument(
					std::format(
						"{}: bad chunk size received, expected it to be cleanly divisible by {} but instead got {:L}",
						context, sizeof(T), chunk_size
					)
				);
			}
		}

		inline uint8_t ensure_compression_level(size_t compression_level)
		{
			if (compression_level > 9)
			{
				std::cout << "Blosc2 only supports compression levels from 0-9, truncating value to this" << std::endl;
				compression_level = 9;
			}
			return static_cast<uint8_t>(compression_level);
		}

		template <typename T>
		std::span<const T> as_const_span(std::vector<T> data)
		{
			return std::span<const T>(data.begin(), data.end());
		}

		template <typename T>
		std::span<const T> as_const_span(std::span<T> data)
		{
			return std::span<const T>(data.begin(), data.end());
		}

		/// Align the given tparam ChunkSize to be a multiple of `width`. This simplifies
		/// a lot of the calculations around chunk size so should be used instead of always
		/// aligning to the chunk size.
		/// 
		/// \tparam T the type representing the image data
		/// \tparam ChunkSize the size of a single chunk (in bytes)
		/// \param width the width of a single scanline (in elements)
		template <typename T, size_t ChunkSize>
		size_t align_chunk_to_scanlines(size_t width)
		{
			// The flooring here is intentional, we want to exclude any partial scanlines.
			size_t num_scanlines = ChunkSize / sizeof(T) / width;
			if (num_scanlines == 0)
			{
				throw std::runtime_error(
					std::format(
						"Unable to align chunk size to scanlines as the size of a scanline exceeds the chunk size."
						" Got a scanline size of {:L} while the max size of the chunks is {:L}"
						, width, ChunkSize
					)
				);
			}
			return num_scanlines * width;
		}

		/// Checks if the given `sz` is aligned to a multiple of the scanline size.
		/// Intended to be used to validate that the data is as expected.
		///
		/// \tparam T the type representing the image data
		/// \param byte_size The total size in bytes
		/// \param width the width of a single scanline (in elements)
		template <typename T>
		constexpr bool is_aligned_to_scanlines(size_t byte_size, size_t width)
		{
			return byte_size % (width * sizeof(T)) == 0;
		}
	}


} // NAMESPACE_COMPRESSED_IMAGE