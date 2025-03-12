#pragma once

#include "macros.h"
#include "constants.h"

namespace NAMESPACE_COMPRESSED_IMAGE
{

	template <typename T, size_t BlockSize = s_default_blocksize, size_t ChunkSize = s_default_chunksize>
	struct image;

} // NAMESPACE_COMPRESSED_IMAGE