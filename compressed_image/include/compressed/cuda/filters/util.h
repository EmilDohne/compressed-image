#pragma once

#include <cuda_runtime.h>

#include "compressed/macros.h"

namespace
NAMESPACE_COMPRESSED_IMAGE
{
    namespace cuda::detail
    {
        /// \brief default function signature for all filter implementations.
        ///
        /// This is the default invocation signature for both forward/backward filter implementations,
        /// operating directly on device memory.
        using filter_function_t = cudaError_t(*)(
            const uint8_t* /* device_input_ptr */,
            uint8_t* /* device_output_ptr */,
            size_t /* length (bytes) */,
            size_t /* type_size */,
            size_t /* row_stride (elements); 0 = treat the whole chunk as one row (no 2D reset) */,
            cudaStream_t /* stream */
        );
    }
}
