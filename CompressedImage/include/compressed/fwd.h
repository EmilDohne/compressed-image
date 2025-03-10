#pragma once

#include "macros.h"


template <typename T, size_t _Block_Size = 32'768, size_t _Chunk_Size = 16'777'216>
struct NAMESPACE_COMPRESSED_IMAGE::image;


template <typename T, std::size_t N>
struct NAMESPACE_COMPRESSED_IMAGE::strided_span_group;