#pragma once

#include <variant>

#include "blosc2/wrapper.h"
#include "compressed/macros.h"
#include "cuda/compressors/base.h"

namespace
NAMESPACE_COMPRESSED_IMAGE
{
    struct cpu_compression_context
    {
        blosc2::context_ptr compression_ctx = nullptr;
        blosc2::context_ptr decompression_ctx = nullptr;

        size_t nthreads{};
    };

    struct gpu_compression_context
    {
        cuda::nvcomp_context ctx{};
    };

    using compression_context_var = std::variant<cpu_compression_context, gpu_compression_context>;
} // NAMESPACE_COMPRESSED_IMAGE
