#pragma once

#include "compressed/macros.h"
#include "compressed/cuda/cuda_hook.h"

namespace
NAMESPACE_COMPRESSED_IMAGE::gpu
{
    [[nodiscard]] inline bool is_available() noexcept
    {
        try
        {
            auto& inst = cuda::cuda_api::instance();
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
} // namespace NAMESPACE_COMPRESSED_IMAGE::gpu
