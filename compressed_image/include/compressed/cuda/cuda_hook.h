/*
Dynamic function hook for cuda that loads the library at runtime and hooks various functions such as 
cudaMalloc, cudaFree, etc.

Unfortunately, it doesn't seem as though there's an open source library so we do the minimal hooking here.

Note: This header should only ever be included on a machine that also has the cuda libraries!
*/

#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <limits>

#include <cuda_runtime.h>

#include "compressed/logger.h"
#include "compressed/macros.h"
#include "compressed/cuda/proc_util.h"


namespace
NAMESPACE_COMPRESSED_IMAGE
{
    namespace cuda
    {
        /// \brief Singleton class for dynamically loading CUDA functions at runtime.
        ///
        /// This allows calling CUDA functions like cudaMalloc/cudaFree
        /// without linking against CUDA at compile time.
        ///
        /// Usage:
        ///
        /// compressed::cuda::cuda_api::instance().malloc(ptr, size);
        /// compressed::cuda::cuda_api::instance().free(ptr);
        /// \brief Singleton for dynamically loading CUDA runtime functions.
        class cuda_api
        {
        public:
            /// Access the singleton instance
            static cuda_api& instance()
            {
                static cuda_api inst;
                return inst;
            }

            using alloc_callback_t = std::function<void(void* ptr, size_t size)>;
            using free_callback_t = std::function<void(void* ptr)>;

            // --- Callback Registration ---
            void set_alloc_callback(alloc_callback_t cb) { alloc_cb_ = std::move(cb); }
            void set_free_callback(free_callback_t cb) { free_cb_ = std::move(cb); }

            // --- Runtime queries ---
            bool available() const noexcept { return handle_ != nullptr; }
            std::string get_error_string(const cudaError_t error) const;
            int device_count() const;
            int current_device() const;
            void set_device(int device);
            bool has_device() const;
            cudaDeviceProp device_properties(int device) const;
            int device_attribute(cudaDeviceAttr attr, int device) const;

            // --- Memory management ---
            void malloc(void*& ptr, size_t size) const;
            void malloc_host(void*& ptr, size_t size) const;
            void malloc_async(void*& ptr, size_t size, cudaStream_t stream = cudaStreamPerThread);
            void free(void* ptr) const;
            void free_host(void* ptr) const;
            void free_async(void* ptr, cudaStream_t stream = cudaStreamPerThread);

            // --- Memory Queries ---
            void mem_get_info(size_t& free_bytes, size_t& total_bytes) const;
            uint64_t pool_allocated_bytes(int device) const;

            // --- Page-locking (Pinning) ---
            void host_register(void* ptr, size_t size, unsigned int flags = cudaHostRegisterDefault) const;
            void host_unregister(void* ptr) const;

            // --- Data transfer ---
            void memcpy(void* dst, const void* src, size_t count, cudaMemcpyKind kind);
            void memcpy_async(
                void* dst,
                const void* src,
                size_t count,
                cudaMemcpyKind kind,
                cudaStream_t stream = cudaStreamPerThread
            );

            // --- Streams & Pools ---
            void stream_synchronize(cudaStream_t stream) const;
            void set_mem_pool_size(int device, uint64_t threshold = std::numeric_limits<uint64_t>::max());

            // Non-copyable
            cuda_api(const cuda_api&) = delete;
            cuda_api& operator=(const cuda_api&) = delete;
            cuda_api(cuda_api&&) = delete;
            cuda_api& operator=(cuda_api&&) = delete;

        private:
            mutable alloc_callback_t alloc_cb_;
            mutable free_callback_t free_cb_;

            // Private constructor
            cuda_api();

            template <typename Func, typename... Args>
            static void cuda_call(Func func, std::string_view func_name, Args&&... args);

            // CUDA library handle
            proc::library_handle handle_ = nullptr;

            // --- Function pointer typedefs ---
            using cuda_malloc_t = cudaError_t(*)(void**, size_t);
            using cuda_malloc_async_t = cudaError_t(*)(void**, size_t, cudaStream_t);
            using cuda_free_t = decltype(&cudaFree);
            using cuda_free_async_t = decltype(&cudaFreeAsync);
            using cuda_malloc_host_t = cudaError_t(*)(void**, size_t);
            using cuda_free_host_t = decltype(&cudaFreeHost);

            using cuda_mem_get_info_t = decltype(&cudaMemGetInfo);

            using cuda_host_register_t = decltype(&cudaHostRegister);
            using cuda_host_unregister_t = decltype(&cudaHostUnregister);;

            using cuda_memcpy_t = decltype(&cudaMemcpy);
            using cuda_memcpy_async_t = decltype(&cudaMemcpyAsync);
            using cuda_stream_sync_t = decltype(&cudaStreamSynchronize);

            using cuda_get_mempool_t = decltype(&cudaDeviceGetDefaultMemPool);
            using cuda_set_mempool_t = decltype(&cudaMemPoolSetAttribute);

            using cuda_set_device_t = decltype(&cudaSetDevice);
            using cuda_get_device_count_t = decltype(&cudaGetDeviceCount);
            using cuda_get_props_t = decltype(&cudaGetDeviceProperties);
            using cuda_get_device_t = decltype(&cudaGetDevice);
            using cuda_get_attr_t = decltype(&cudaDeviceGetAttribute);

            using cuda_get_error_str_t = decltype(&cudaGetErrorString);

            // --- Function pointers ---
            cuda_malloc_t malloc_fn_ = nullptr;
            cuda_malloc_host_t malloc_host_fn_ = nullptr;
            cuda_malloc_async_t malloc_async_fn_ = nullptr;
            cuda_free_t free_fn_ = nullptr;
            cuda_free_host_t free_host_fn_ = nullptr;
            cuda_free_async_t free_async_fn_ = nullptr;

            cuda_mem_get_info_t mem_get_info_fn_ = nullptr;

            cuda_host_register_t host_register_fn_ = nullptr;
            cuda_host_unregister_t host_unregister_fn_ = nullptr;

            cuda_memcpy_t memcpy_fn_ = nullptr;
            cuda_memcpy_async_t memcpy_async_fn_ = nullptr;
            cuda_stream_sync_t stream_sync_fn_ = nullptr;

            cuda_get_mempool_t get_mempool_fn_ = nullptr;
            cuda_set_mempool_t set_mempool_fn_ = nullptr;

            cuda_set_device_t set_device_fn_ = nullptr;
            cuda_get_device_count_t get_count_fn_ = nullptr;
            cuda_get_props_t get_props_fn_ = nullptr;
            cuda_get_device_t get_device_fn_ = nullptr;
            cuda_get_attr_t get_attr_fn_ = nullptr;

            cuda_get_error_str_t get_error_str_fn_ = nullptr;
        };

        // ===========================================================
        // Implementation
        // ===========================================================

        inline std::string cuda_api::get_error_string(const cudaError_t error) const
        {
            _COMPRESSED_PROFILE_FUNCTION();
            if (!get_error_str_fn_)
            {
                throw std::runtime_error(
                    "CUDA function 'cudaGetErrorString' is unavailable (library or entrypoint not loaded)."
                );
            }

            try
            {
                return std::string(get_error_str_fn_(error));
            }
            catch (...)
            {
                return std::format(
                    "unknown error or missing driver string table. Cuda code {}",
                    static_cast<int>(error)
                );
            }
        }

        inline int cuda_api::device_count() const
        {
            _COMPRESSED_PROFILE_FUNCTION();
            int count = 0;
            cuda_call(get_count_fn_, "cudaGetDeviceCount", &count);
            return count;
        }

        inline int cuda_api::current_device() const
        {
            _COMPRESSED_PROFILE_FUNCTION();
            int dev = -1;
            cuda_call(get_device_fn_, "cudaGetDevice", &dev);
            return dev;
        }

        inline void cuda_api::set_device(int device)
        {
            _COMPRESSED_PROFILE_FUNCTION();
            cuda_call(set_device_fn_, "cudaSetDevice", device);
        }

        inline bool cuda_api::has_device() const
        {
            _COMPRESSED_PROFILE_FUNCTION();
            if (!available()) return false;

            try
            {
                return device_count() > 0;
            }
            catch (const std::exception& e)
            {
                NAMESPACE_COMPRESSED_IMAGE::get_logger()->warn(
                    std::format(
                        "Unhandled exception while trying to retrieve the cuda device count: {}",
                        e.what()
                    )
                );
            }

            return false;
        }

        inline cudaDeviceProp cuda_api::device_properties(int device) const
        {
            _COMPRESSED_PROFILE_FUNCTION();
            cudaDeviceProp prop{};
            cuda_call(get_props_fn_, "cudaGetDeviceProperties", &prop, device);
            return prop;
        }

        inline int cuda_api::device_attribute(cudaDeviceAttr attr, int device) const
        {
            _COMPRESSED_PROFILE_FUNCTION();
            int value = 0;
            cuda_call(get_attr_fn_, "cudaDeviceGetAttribute", &value, attr, device);
            return value;
        }

        inline void cuda_api::mem_get_info(size_t& free_bytes, size_t& total_bytes) const
        {
            cuda_call(mem_get_info_fn_, "cudaMemGetInfo", &free_bytes, &total_bytes);
        }


        inline void cuda_api::malloc(void*& ptr, size_t size) const
        {
            cuda_call(malloc_fn_, "cudaMalloc", &ptr, size);
            if (this->alloc_cb_)
            {
                alloc_cb_(ptr, size);
            }
        }

        inline void cuda_api::malloc_host(void*& ptr, size_t size) const
        {
            _COMPRESSED_PROFILE_FUNCTION();
            cuda_call(malloc_host_fn_, "cudaMallocHost", &ptr, size);
        }

        inline void cuda_api::malloc_async(void*& ptr, size_t size, cudaStream_t stream)
        {
            cuda_call(malloc_async_fn_, "cudaMallocAsync", &ptr, size, stream);
            if (this->alloc_cb_)
            {
                alloc_cb_(ptr, size);
            }
        }

        inline void cuda_api::free(void* ptr) const
        {
            cuda_call(free_fn_, "cudaFree", ptr);
            if (this->free_cb_)
            {
                free_cb_(ptr);
            }
        }

        inline void cuda_api::free_host(void* ptr) const
        {
            _COMPRESSED_PROFILE_FUNCTION();
            cuda_call(free_host_fn_, "cudaFreeHost", ptr);
        }

        inline void cuda_api::free_async(void* ptr, cudaStream_t stream)
        {
            cuda_call(free_async_fn_, "cudaFreeAsync", ptr, stream);
            if (this->free_cb_)
            {
                free_cb_(ptr);
            }
        }

        inline void cuda_api::host_register(void* ptr, size_t size, unsigned int flags) const
        {
            _COMPRESSED_PROFILE_FUNCTION();
            cuda_call(host_register_fn_, "cudaHostRegister", ptr, size, flags);
        }

        inline void cuda_api::host_unregister(void* ptr) const
        {
            _COMPRESSED_PROFILE_FUNCTION();
            cuda_call(host_unregister_fn_, "cudaHostUnregister", ptr);
        }

        inline void cuda_api::memcpy(void* dst, const void* src, size_t count, cudaMemcpyKind kind)
        {
            cuda_call(memcpy_fn_, "cudaMemcpy", dst, src, count, kind);
        }

        inline void cuda_api::memcpy_async(
            void* dst,
            const void* src,
            size_t count,
            cudaMemcpyKind kind,
            cudaStream_t stream)
        {
            cuda_call(memcpy_async_fn_, "cudaMemcpyAsync", dst, src, count, kind, stream);
        }

        inline void cuda_api::stream_synchronize(cudaStream_t stream) const
        {
            _COMPRESSED_PROFILE_FUNCTION();
            cuda_call(stream_sync_fn_, "cudaStreamSynchronize", stream);
        }

        inline void cuda_api::set_mem_pool_size(int device, uint64_t threshold)
        {
            _COMPRESSED_PROFILE_FUNCTION();
            cudaMemPool_t mempool{};
            cuda_call(get_mempool_fn_, "cudaDeviceGetDefaultMemPool", &mempool, device);
            cuda_call(
                set_mempool_fn_,
                "cudaMemPoolSetAttribute",
                mempool,
                cudaMemPoolAttrReleaseThreshold,
                &threshold
            );
        }

        // --- private helpers ---
        template <typename Func, typename... Args>
        void cuda_api::cuda_call(Func func, std::string_view func_name, Args&&... args)
        {
            if (!func)
            {
                throw std::runtime_error(
                    std::format(
                        "CUDA function '{}' is unavailable (library or entrypoint not loaded).",
                        func_name
                    )
                );
            }

            const cudaError_t err = func(std::forward<Args>(args)...);
            if (err != cudaSuccess)
            {
                auto& inst = instance();
                const char* raw_msg = inst.get_error_str_fn_ ? inst.get_error_str_fn_(err) : nullptr;
                std::string invalid_msg = std::format(
                    "unknown error or missing driver string table. Cuda code {}",
                    static_cast<int>(err)
                );

                std::string_view msg = raw_msg ? std::string_view(raw_msg) : invalid_msg;

                throw std::runtime_error(std::format("{} failed: {}", func_name, msg));
            }
        }

        inline cuda_api::cuda_api()
        {
#if defined(_WIN32)
            // Targets the 64-bit Runtime API for CUDA 12
            const std::string cuda_name = "cudart64_12.dll";
#elif defined(__linux__)
            const std::string cuda_name = "libcudart.so";
#else
            const std::string cuda_name;
#endif

            handle_ = proc::load_library(cuda_name);
            if (!handle_) return;

#define LOAD(fn, member) member = proc::get_symbol<decltype(member)>(handle_, #fn, cuda_name)

            LOAD(cudaMalloc, malloc_fn_);
            LOAD(cudaMallocHost, malloc_host_fn_);
            LOAD(cudaMallocAsync, malloc_async_fn_);
            LOAD(cudaFree, free_fn_);
            LOAD(cudaFreeHost, free_host_fn_);
            LOAD(cudaFreeAsync, free_async_fn_);

            LOAD(cudaMemGetInfo, mem_get_info_fn_);

            LOAD(cudaHostRegister, host_register_fn_);
            LOAD(cudaHostUnregister, host_unregister_fn_);

            LOAD(cudaMemcpy, memcpy_fn_);
            LOAD(cudaMemcpyAsync, memcpy_async_fn_);
            LOAD(cudaStreamSynchronize, stream_sync_fn_);

            LOAD(cudaDeviceGetDefaultMemPool, get_mempool_fn_);
            LOAD(cudaMemPoolSetAttribute, set_mempool_fn_);

            LOAD(cudaSetDevice, set_device_fn_);
            LOAD(cudaGetDeviceCount, get_count_fn_);
            LOAD(cudaGetDeviceProperties, get_props_fn_);
            LOAD(cudaGetDevice, get_device_fn_);
            LOAD(cudaDeviceGetAttribute, get_attr_fn_);

            LOAD(cudaGetErrorString, get_error_str_fn_);

#undef LOAD
        }
    } // namespace cuda
} // namespace NAMESPACE_COMPRESSED_IMAGE
