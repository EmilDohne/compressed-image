#pragma once

#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>

#include <benchmark/benchmark.h>

#include "memory_tracker.h"
#include "gpu_memory_tracker.h"


namespace bench_util
{
    namespace detail
    {
        inline constexpr size_t s_mem_sampling_interval = 25;

        // Periodic memory sampling variables
        inline std::atomic<bool> g_sampling{false};
        inline std::mutex g_mem_mutex;
        inline std::vector<size_t> g_memory_samples;
        inline std::vector<size_t> g_vram_samples;
    } // detail

    void memory_profiler()
    {
        while (detail::g_sampling)
        {
            size_t mem_used = MemoryAllocTracker::get_bytes_used();
            size_t vram_used = CudaMemoryTracker::get_bytes_used();

            {
                std::lock_guard<std::mutex> lock(detail::g_mem_mutex);
                detail::g_memory_samples.push_back(mem_used);
                detail::g_vram_samples.push_back(vram_used); // Added: Record VRAM
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(detail::s_mem_sampling_interval));
        }
    }

    // Generic function to run a benchmarked function with memory sampling
    template <typename Func>
    void run_with_memory_sampling(benchmark::State& state, Func&& func)
    {
        detail::g_memory_samples.clear();
        detail::g_sampling = true;
        std::thread mem_thread(memory_profiler); // Start memory profiler

        for (auto _ : state)
        {
            func();
        }

        // Stop memory profiling
        detail::g_sampling = false;
        if (mem_thread.joinable())
        {
            mem_thread.join();
        }

        // Analyze memory samples
        size_t min_mem = std::numeric_limits<size_t>::max();
        size_t max_mem = 0;
        size_t total_mem = 0;

        size_t gpu_min_mem = std::numeric_limits<size_t>::max();
        size_t gpu_max_mem = 0;
        size_t gpu_total_mem = 0;
        {
            std::lock_guard<std::mutex> lock(detail::g_mem_mutex);
            for (size_t mem : detail::g_memory_samples)
            {
                min_mem = std::min(min_mem, mem);
                max_mem = std::max(max_mem, mem);
                total_mem += mem;
            }
            for (size_t mem : detail::g_vram_samples)
            {
                gpu_min_mem = std::min(gpu_min_mem, mem);
                gpu_max_mem = std::max(gpu_max_mem, mem);
                gpu_total_mem += mem;
            }
        }

        state.counters["memory_min_mb"] = static_cast<double>(min_mem) / 1024 / 1024;
        state.counters["memory_max_mb"] = static_cast<double>(max_mem) / 1024 / 1024;
        state.counters["memory_avg_mb"] = static_cast<double>(total_mem) / (1024 * 1024 * detail::g_memory_samples.
            size());

        state.counters["gpu_memory_min_mb"] = static_cast<double>(gpu_min_mem) / 1024 / 1024;
        state.counters["gpu_memory_max_mb"] = static_cast<double>(gpu_max_mem) / 1024 / 1024;
        state.counters["gpu_memory_avg_mb"] = static_cast<double>(gpu_total_mem) / (1024 * 1024 * detail::g_vram_samples
            .size());
    }
} // bench_util
