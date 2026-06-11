#pragma once

#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>
#include <vector>
#include <limits>
#include <algorithm>

#include <benchmark/benchmark.h>

#include "memory_tracker.h"
#include "gpu_memory_tracker.h"

namespace bench_util
{
    namespace detail
    {
        inline constexpr size_t s_mem_sampling_interval_ms = 25;

        // Periodic memory sampling variables
        inline std::atomic<bool> g_sampling{false};
        inline std::mutex g_mem_mutex;

        // Tracks the active baseline floor updated right before each execution loop
        inline std::atomic<size_t> g_base_memory{0};
        inline std::atomic<size_t> g_base_vram{0};

        // Store relative differences (deltas) instead of absolute values
        inline std::vector<int64_t> g_memory_diffs;
        inline std::vector<int64_t> g_vram_usage;
    } // detail

    void memory_profiler()
    {
        while (detail::g_sampling)
        {
            size_t mem_used = memory_tracker::get_bytes_used();
            size_t vram_used = cuda_memory_tracker::get_bytes_used();

            // Load the most recent baseline floor set by the main execution thread
            size_t base_mem = detail::g_base_memory.load(std::memory_order_relaxed);

            int64_t mem_diff = static_cast<int64_t>(mem_used) - static_cast<int64_t>(base_mem);

            {
                std::lock_guard<std::mutex> lock(detail::g_mem_mutex);
                detail::g_memory_diffs.push_back(mem_diff);
                detail::g_vram_usage.push_back(vram_used);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(detail::s_mem_sampling_interval_ms));
        }
    }

    // Generic function to run a benchmarked function with memory sampling
    template <typename Func>
    void run_with_memory_sampling(benchmark::State& state, Func&& func)
    {
        {
            std::lock_guard<std::mutex> lock(detail::g_mem_mutex);
            detail::g_memory_diffs.clear();
            detail::g_vram_usage.clear();
        }

        // Establish an initial baseline floor before thread startup
        detail::g_base_memory.store(memory_tracker::get_bytes_used(), std::memory_order_relaxed);

        detail::g_sampling = true;
        std::thread mem_thread(memory_profiler); // Start memory profiler

        for (auto _ : state)
        {
            // Pause benchmark timer so memory floor calculations do not skew execution time metrics
            state.PauseTiming();
            detail::g_base_memory.store(memory_tracker::get_bytes_used(), std::memory_order_relaxed);
            state.ResumeTiming();

            func();
        }

        // Stop memory profiling
        detail::g_sampling = false;
        if (mem_thread.joinable())
        {
            mem_thread.join();
        }

        // Analyze memory sample differences
        int64_t min_mem_diff = std::numeric_limits<int64_t>::max();
        int64_t max_mem_diff = std::numeric_limits<int64_t>::min();
        int64_t total_mem_diff = 0;

        size_t min_vram = std::numeric_limits<size_t>::max();
        size_t max_vram = std::numeric_limits<size_t>::min();
        size_t total_vram = 0;

        size_t mem_samples_count = 0;
        size_t vram_samples_count = 0;

        {
            std::lock_guard<std::mutex> lock(detail::g_mem_mutex);
            mem_samples_count = detail::g_memory_diffs.size();
            vram_samples_count = detail::g_vram_usage.size();

            for (int64_t diff : detail::g_memory_diffs)
            {
                min_mem_diff = std::min(min_mem_diff, diff);
                max_mem_diff = std::max(max_mem_diff, diff);
                total_mem_diff += diff;
            }
            for (size_t usage : detail::g_vram_usage)
            {
                min_vram = std::min(min_vram, usage);
                max_vram = std::max(max_vram, usage);
                total_vram += usage;
            }
        }

        constexpr double mb_divisor = 1024.0 * 1024.0;

        if (mem_samples_count > 0)
        {
            state.counters["mem_min_mb"] = static_cast<double>(min_mem_diff) / mb_divisor;
            state.counters["mem_max_mb"] = static_cast<double>(max_mem_diff) / mb_divisor;
            state.counters["mem_avg_mb"] = static_cast<double>(total_mem_diff) / (mb_divisor * mem_samples_count);
        }

        if (vram_samples_count > 0)
        {
            state.counters["gpu_min_mb"] = static_cast<double>(min_vram) / mb_divisor;
            state.counters["gpu_max_mb"] = static_cast<double>(max_vram) / mb_divisor;
            state.counters["gpu_avg_mb"] = static_cast<double>(total_vram) / (mb_divisor * vram_samples_count);
        }
    }
} // bench_util
