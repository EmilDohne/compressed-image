#pragma once

#include <memory>

#include "macros.h"

#include "blosc2.h"


namespace NAMESPACE_COMPRESSED_IMAGE
{

	namespace blosc2
	{

		// Custom deleter for blosc2 structs for use in a smart pointer
		template <typename T>
		struct deleter {};

		template <>
		struct deleter<blosc2_schunk>
		{
			void operator()(blosc2_schunk* schunk)
			{
				blosc2_schunk_free(schunk);
			}
		};

		template <>
		struct deleter<blosc2_context>
		{
			void operator()(blosc2_context* context)
			{
				blosc2_free_ctx(context);
			}
		};


		typedef schunk_ptr		= std::unique_ptr<blosc2_schunk, deleter<blosc2_schunk>>;
		/// Raw pointer to a blosc2_schunk. Intended to be used as a view over a `schunk_ptr`
		/// with ownership staying with the `schunk_ptr`
		typedef schunk_raw_ptr	= blosc2_schunk*;

		typedef chunk_raw_ptr	= void*;

		typedef context_ptr		= std::unique_ptr<blosc2_context, deleter<blosc2_context>>;
		/// Raw pointer to a blosc2_context. Intended to be used as a view over a `context_ptr`
		/// with ownership staying with the `context_ptr`
		typedef context_raw_ptr = blosc2_context*;


	} // namespace blosc2


} // NAMESPACE_COMPRESSED_IMAGE