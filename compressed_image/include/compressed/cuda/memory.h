/*
Wrapper around cuda memory allocation/deallocation using std::unique_ptr to manage freeing memory appropriately
again instead of having to do this by hand
*/
#pragma once

#include <memory>

#include <cuda_runtime.h>

#include "compressed/macros.h"
#include "compressed/util.h"
#include "compressed/cuda/cuda_hook.h"


namespace
NAMESPACE_COMPRESSED_IMAGE
{
    namespace cuda
    {
        namespace detail
        {
            struct device_deleter
            {
                void operator()(void* ptr) const noexcept
                {
                    if (ptr)
                    {
                        try
                        {
                            cuda_api::instance().free(ptr);
                        }
                        catch (...)
                        {
                            // suppress exceptions in destructors
                        }
                    }
                }
            };

            struct device_deleter_async
            {
                // Must be the same stream used for construction, use the factory functions to ensure this holds
                cudaStream_t stream = cudaStreamPerThread;

                void operator()(void* ptr) const noexcept
                {
                    if (ptr)
                    {
                        try
                        {
                            cuda_api::instance().free_async(ptr, stream);
                        }
                        catch (...)
                        {
                            // suppress exceptions in destructors
                        }
                    }
                }
            };

            struct host_deleter
            {
                void operator()(void* ptr) const noexcept
                {
                    if (ptr)
                    {
                        try
                        {
                            cuda_api::instance().free_host(ptr);
                        }
                        catch (...)
                        {
                            // suppress exceptions in destructors
                        }
                    }
                }
            };
        } // namespace detail

        // -------------------------------------------------------------------------
        // Smart pointer aliases (void*, untyped)
        // -------------------------------------------------------------------------
        using cuda_device_mem = std::unique_ptr<void, detail::device_deleter>;
        using cuda_device_mem_async = std::unique_ptr<void, detail::device_deleter_async>;
        using cuda_host_mem = std::unique_ptr<void, detail::host_deleter>;

        // -------------------------------------------------------------------------
        // Allocation helpers (typed)
        // -------------------------------------------------------------------------
        template <typename T>
        using cuda_device_ptr = std::unique_ptr<T, detail::device_deleter>;

        template <typename T>
        struct cuda_device_buffer
        {
            cuda_device_ptr<T> data = nullptr;
            size_t size{};

            T* get() noexcept { return this->data.get(); }
            const T* get() const noexcept { return this->data.get(); }
            void* get_raw() noexcept { return static_cast<void*>(this->get()); }
            [[nodiscard]] const void* get_raw() const noexcept { return static_cast<const void*>(this->get()); }

            [[nodiscard]] size_t bytes() const noexcept { return this->size * sizeof(T); }
        };

        template <typename T>
        using cuda_device_ptr_async = std::unique_ptr<T, detail::device_deleter_async>;

        template <typename T>
        struct cuda_device_buffer_async
        {
            /// \brief the underlying raw device ptr.
            cuda_device_ptr_async<T> data = nullptr;
            /// \brief the number of elements in the device buffer (expressed as a multiple of T)
            size_t size{};

            /// \brief Generate a device buffer (using asynchronous memory ops) from a host buffer copying the data.
            ///
            /// \param buffer The buffer to use as a size reference and to generate the device pointer from
            static cuda_device_buffer_async from_host(std::span<const T> buffer)
            {
                void* raw = nullptr;

                cuda_api::instance().malloc_async(
                    raw,
                    buffer.size() * sizeof(T),
                    cudaStreamPerThread
                );

                auto gpu_buffer = cuda_device_buffer_async<T>{
                    cuda_device_ptr_async<T>(
                        static_cast<T*>(raw),
                        detail::device_deleter_async{cudaStreamPerThread}
                    ),
                    buffer.size()
                };

                cuda_api::instance().memcpy_async(
                    static_cast<void*>(gpu_buffer.data.get()),
                    buffer.data(),
                    gpu_buffer.bytes(),
                    cudaMemcpyHostToDevice
                );

                return gpu_buffer;
            }

            static cuda_device_buffer_async from_host(std::vector<T>& buffer)
            {
                return cuda_device_buffer_async::from_host(std::span<T>(buffer.begin(), buffer.end()));
            }

            void to_host(std::span<T> buffer)
            {
                if (buffer.size() != this->size)
                {
                    throw std::invalid_argument(
                        std::format(
                            "Cuda: Invalid buffer passed to `to_host` function. Expected exactly {} elements but instead"
                            " got {}.",
                            this->size,
                            buffer.size()
                        )
                    );
                }

                cuda_api::instance().memcpy_async(
                    static_cast<void*>(buffer.data()),
                    this->get_raw(),
                    this->size * sizeof(T),
                    cudaMemcpyHostToDevice
                );
            }

            /// \brief allocate and memcpy the compressed data back to the host.
            NAMESPACE_COMPRESSED_IMAGE::util::default_init_vector<T> to_host()
            {
                util::default_init_vector<T> buffer(this->size);
                this->to_host(std::span<T>(buffer.begin(), buffer.end()));
                return buffer;
            }

            T* get() noexcept { return this->data.get(); }
            const T* get() const noexcept { return this->data.get(); }
            void* get_raw() noexcept { return static_cast<void*>(this->get()); }
            [[nodiscard]] const void* get_raw() const noexcept { return static_cast<const void*>(this->get()); }

            [[nodiscard]] size_t bytes() const noexcept { return this->size * sizeof(T); }
        };

        template <typename T>
        using cuda_host_ptr = std::unique_ptr<T, detail::host_deleter>;

        // -------------------------------------------------------------------------
        // Factory functions, use these whenever possible!
        // -------------------------------------------------------------------------
        template <typename T = void>
        inline cuda_device_ptr<T> make_device_mem(size_t count)
        {
            void* raw = nullptr;
            cuda_api::instance().malloc(raw, count * sizeof(T));
            return cuda_device_ptr<T>(static_cast<T*>(raw));
        }

        template <typename T = void>
        inline cuda_device_buffer<T> make_device_buffer(size_t count)
        {
            auto managed_ptr = make_device_mem<T>(count);
            return cuda_device_buffer<T>{std::move(managed_ptr), count};
        }

        template <typename T = void>
        inline cuda_device_ptr_async<T> make_device_mem_async(size_t count, cudaStream_t stream = cudaStreamPerThread)
        {
            void* raw = nullptr;
            cuda_api::instance().malloc_async(raw, count * sizeof(T), stream);
            return cuda_device_ptr_async<T>(static_cast<T*>(raw), detail::device_deleter_async{stream});
        }

        template <typename T = void>
        inline cuda_device_buffer_async<T> make_device_buffer_async(size_t count,
                                                                    cudaStream_t stream = cudaStreamPerThread)
        {
            auto managed_ptr = make_device_mem_async<T>(count, stream);
            return cuda_device_buffer_async<T>{std::move(managed_ptr), count};
        }

        template <typename T = void>
        inline cuda_host_ptr<T> make_host_mem(size_t count)
        {
            void* raw = nullptr;
            cuda_api::instance().malloc_host(raw, count * sizeof(T));
            return cuda_host_ptr<T>(static_cast<T*>(raw));
        }
    }
} // namespace NAMESPACE_COMPRESSED_IMAGE
