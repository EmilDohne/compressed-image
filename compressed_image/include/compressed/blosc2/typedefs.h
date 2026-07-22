#pragma once

#include "compressed/macros.h"

#include "schunk_mixin.h"
#include "lazyschunk.h"
#include "schunk.h"

namespace NAMESPACE_COMPRESSED_IMAGE
{
		
	template <typename T>
	using schunk_var_ptr = std::shared_ptr<std::variant<detail::schunk<T>, detail::lazy_schunk<T>>>;
	template <typename T>
	using schunk_var = std::variant<detail::schunk<T>, detail::lazy_schunk<T>>;

} // NAMESPACE_COMPRESSED_IMAGE