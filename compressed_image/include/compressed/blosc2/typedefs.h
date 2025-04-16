#pragma once

#include "compressed/macros.h"

#include "schunk_mixin.h"
#include "lazyschunk.h"
#include "schunk.h"

namespace NAMESPACE_COMPRESSED_IMAGE
{

	namespace blosc2
	{
		
		template <typename T>
		using schunk_var_ptr = std::shared_ptr<std::variant<blosc2::schunk<T>, blosc2::lazy_schunk<T>>>;
		template <typename T>
		using schunk_var = std::variant<blosc2::schunk<T>, blosc2::lazy_schunk<T>>;

	} // blosc2

} // NAMESPACE_COMPRESSED_IMAGE