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
        inline constexpr size_t s_mem_sampling_interval_ms = 25;

        // Periodic memory sampling variables
        inline std::atomic<bool> g_sampling{false};
        inline std::mutex g_mem_mutex;

        // Tracks the active baseline floor updated right before each execution loop
        inline std::atomic<size_t> g_base_memory{0};
        inline std::atomic<size_t> g_base_vram{0};

        // Store relative differences (deltas) instead of absolute values
        inline std::vector<int64_t> g_memory_diffs;
        inline std::vector<int64_t> g_vram_diffs;
    } // detail

    void memory_profiler()
    {
        while (detail::g_sampling)
        {
            size_t mem_used = MemoryAllocTracker::get_bytes_used();
            size_t vram_used = CudaMemoryTracker::get_bytes_used();

            // Load the most recent baseline floor set by the main execution thread
            size_t base_mem = detail::g_base_memory.load(std::memory_order_relaxed);
            size_t base_vram = detail::g_base_vram.load(std::memory_order_relaxed);

            // Compute signed differences (handles potential deallocations gracefully)
            int64_t mem_diff = static_cast<int64_t>(mem_used) - static_cast<int64_t>(base_mem);
            int64_t vram_diff = static_cast<int64_t>(vram_used) - static_cast<int64_t>(base_vram);

            {
                std::lock_guard<std::mutex> lock(detail::g_mem_mutex);
                detail::g_memory_diffs.push_back(mem_diff);
                detail::g_vram_diffs.push_back(vram_diff);
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
            detail::g_vram_diffs.clear();
        }

        // Establish an initial baseline floor before thread startup
        detail::g_base_memory.store(MemoryAllocTracker::get_bytes_used(), std::memory_order_relaxed);
        detail::g_base_vram.store(CudaMemoryTracker::get_bytes_used(), std::memory_order_relaxed);

        detail::g_sampling = true;
        std::thread mem_thread(memory_profiler); // Start memory profiler

        for (auto _ : state)
        {
            // Pause benchmark timer so memory floor calculations do not skew execution time metrics
            state.PauseTiming();
            detail::g_base_memory.store(MemoryAllocTracker::get_bytes_used(), std::memory_order_relaxed);
            detail::g_base_vram.store(CudaMemoryTracker::get_bytes_used(), std::memory_order_relaxed);
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

        int64_t min_vram_diff = std::numeric_limits<int64_t>::max();
        int64_t max_vram_diff = std::numeric_limits<int64_t>::min();
        int64_t total_vram_diff = 0;

        size_t mem_samples_count = 0;
        size_t vram_samples_count = 0;

        {
            std::lock_guard<std::mutex> lock(detail::g_mem_mutex);
            mem_samples_count = detail::g_memory_diffs.size();
            vram_samples_count = detail::g_vram_diffs.size();

            for (int64_t diff : detail::g_memory_diffs)
            {
                min_mem_diff = std::min(min_mem_diff, diff);
                max_mem_diff = std::max(max_mem_diff, diff);
                total_mem_diff += diff;
            }
            for (int64_t diff : detail::g_vram_diffs)
            {
                min_vram_diff = std::min(min_vram_diff, diff);
                max_vram_diff = std::max(max_vram_diff, diff);
                total_vram_diff += diff;
            }
        }

        constexpr double mb_divisor = 1024.0 * 1024.0;

        if (mem_samples_count > 0)
        {
            state.counters["mem_diff_min_mb"] = static_cast<double>(min_mem_diff) / mb_divisor;
            state.counters["mem_diff_max_mb"] = static_cast<double>(max_mem_diff) / mb_divisor;
            state.counters["mem_diff_avg_mb"] = static_cast<double>(total_mem_diff) / (mb_divisor * mem_samples_count);
        }

        if (vram_samples_count > 0)
        {
            state.counters["gpu_diff_min_mb"] = static_cast<double>(min_vram_diff) / mb_divisor;
            state.counters["gpu_diff_max_mb"] = static_cast<double>(max_vram_diff) / mb_divisor;
            state.counters["gpu_diff_avg_mb"] = static_cast<double>(total_vram_diff) / (mb_divisor *
                vram_samples_count);
        }
    }
} // bench_util
