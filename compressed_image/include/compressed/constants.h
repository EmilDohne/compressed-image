#pragma once

#include "macros.h"


namespace NAMESPACE_COMPRESSED_IMAGE
{
	/// Default chunk size for blosc2 super-chunks. This equates to 4MB or one 2048*2048 channel
	// constexpr static inline std::size_t s_default_chunksize = 4'194'304;
	constexpr static inline std::size_t s_default_chunksize = 268'435'456;
	/// Default block size for blosc2 chunks. This equates to 16 scanlines in that same 2048*2048 channel.
	constexpr static inline std::size_t s_default_blocksize = 32'768;

} // NAMESPACE_COMPRESSED_IMAGE