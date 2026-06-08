#pragma once

#include "compressed/cuda/cuda_hook.h"

namespace bench_util
{
    struct CudaMemoryTracker
    {
        /// \brief Returns global VRAM currently consumed across the active GPU device.
        /// Note: This represents total system allocation on the card.
        inline static size_t get_bytes_used() noexcept
        {
            const auto& api = NAMESPACE_COMPRESSED_IMAGE::cuda::cuda_api::instance();
            if (!api.available()) return 0;

            try
            {
                size_t free_bytes = 0;
                size_t total_bytes = 0;
                api.mem_get_info(free_bytes, total_bytes);
                return total_bytes - free_bytes;
            }
            catch (...)
            {
                return 0;
            }
        }

        inline static double get_kb_used() noexcept
        {
            return static_cast<double>(get_bytes_used()) / 1024.0;
        }

        inline static double get_mb_used() noexcept
        {
            return static_cast<double>(get_bytes_used()) / 1024.0 / 1024.0;
        }

        inline static double get_gb_used() noexcept
        {
            return static_cast<double>(get_bytes_used()) / 1024.0 / 1024.0 / 1024.0;
        }
    };
} // namespace bench_util
