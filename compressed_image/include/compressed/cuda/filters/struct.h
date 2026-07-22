#pragma once

#include "compressed/cuda/filters/enums.h"

namespace
NAMESPACE_COMPRESSED_IMAGE
{
    namespace cuda
    {
        struct gpu_filter
        {
            cuda::enums::filter type;
        };
    }
}
