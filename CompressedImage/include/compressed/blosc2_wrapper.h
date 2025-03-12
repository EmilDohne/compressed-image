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


		typedef std::unique_ptr<blosc2_schunk, deleter<blosc2_schunk>>		schunk_ptr;
		typedef blosc2_schunk*												schunk_raw_ptr;
		typedef void*														chunk_raw_ptr;
		typedef std::unique_ptr<blosc2_context, deleter<blosc2_context>>	context_ptr;
		typedef blosc2_context*												context_raw_ptr;


	} // namespace blosc2


} // NAMESPACE_COMPRESSED_IMAGE