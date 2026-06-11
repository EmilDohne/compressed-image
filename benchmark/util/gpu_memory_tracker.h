#pragma once

#include <mutex>
#include <unordered_map>
#include <atomic>

#include "compressed/cuda/cuda_hook.h"

class cuda_memory_tracker
{
public:
    // Call this once at the start of your program (e.g., in main())
    static void initialize()
    {
        auto& api = NAMESPACE_COMPRESSED_IMAGE::cuda::cuda_api::instance();

        api.set_alloc_callback(
            [](void* ptr, size_t size)
            {
                std::lock_guard<std::mutex> lock(get_mutex());
                get_map()[ptr] = size;
                get_total_bytes().fetch_add(size, std::memory_order_relaxed);
            }
        );

        api.set_free_callback(
            [](void* ptr)
            {
                std::lock_guard<std::mutex> lock(get_mutex());
                auto& map = get_map();
                auto it = map.find(ptr);
                if (it != map.end())
                {
                    get_total_bytes().fetch_sub(it->second, std::memory_order_relaxed);
                    map.erase(it);
                }
            }
        );
    }

    static size_t get_bytes_used()
    {
        return get_total_bytes().load(std::memory_order_relaxed);
    }

private:
    static std::mutex& get_mutex()
    {
        static std::mutex mtx;
        return mtx;
    }

    static std::unordered_map<void*, size_t>& get_map()
    {
        static std::unordered_map<void*, size_t> map;
        return map;
    }

    static std::atomic<size_t>& get_total_bytes()
    {
        static std::atomic<size_t> total{0};
        return total;
    }
};
