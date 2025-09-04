/*
Dynamic function hook for cuda that loads the library at runtime and hooks various functions such as 
cudaMalloc, cudaFree etc.

Unfortunately it doesn't seem as though there's an open source library so we do the minimal hooking here.

Note: This header should only ever be included on a machine that also has the cuda libraries!
*/

#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <cuda_runtime.h>

#include "compressed/macros.h"
#include "compressed/cuda/exceptions.h"
#include "compressed/cuda/proc_util.h"


namespace NAMESPACE_COMPRESSED_IMAGE
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
		class cuda_api
		{
		public:

			using cuda_malloc_t			= decltype(&cudaMalloc);
			using cuda_malloc_async_t	= decltype(&cudaMallocAsync);
			using cuda_free_t			= decltype(&cudaFree);
			using cuda_free_async_t		= decltype(&cudaFreeAsync);

			using cuda_malloc_host_t	= decltype(&cudaMallocHost);
			using cuda_free_host_t		= decltype(&cudaFreeHost);

			using cuda_memcpy_t			= decltype(&cudaMemcpy);
			using cuda_memcpy_async_t	= decltype(&cudaMemcpyAsync);

			using cuda_device_get_default_mem_pool_t	= decltype(&cudaDeviceGetDefaultMemPool);
			using cuda_mempool_set_attribute_t			= decltype(&cudaMemPoolSetAttribute);

			using cuda_get_error_str_t	= decltype(&cudaGetErrorString);

			/// \brief Access the singleton instance
			/// \return Reference to the cuda_api singleton
			static cuda_api& instance()
			{
				static cuda_api instance;
				return instance;
			}

			/// \brief Allocate memory on GPU via dynamically loaded cudaMalloc
			/// \throw std::runtime_error if allocation fails
			void malloc(void*& ptr, size_t size) const
			{
				cuda_call(cuda_malloc_, "cudaMalloc", &ptr, size);
			}

			void malloc_host(void*& ptr, size_t size) const
			{
				cuda_call(cuda_malloc_host_, "cudaMallocHost", &ptr, size);
			}

			/// \brief Allocate memory on GPU via dynamically loaded cudaMallocAsync
			/// 
			/// Performs this allocation asynchronously with respect to the passed stream, defaulting to
			/// a single stream per thread.
			/// 
			/// \throw std::runtime_error if allocation fails
			void malloc_async(void*& ptr, size_t size, cudaStream_t stream = cudaStreamPerThread)
			{
				cuda_call(cuda_malloc_async_, "cudaMallocAsync", &ptr, size, stream);
			}

			/// \brief Free GPU memory via dynamically loaded cudaFree
			/// \throw std::runtime_error if freeing fails
			void free(void* ptr) const
			{
				cuda_call(cuda_free_, "cudaFree", ptr);
			}

			void free_host(void* ptr) const
			{
				cuda_call(cuda_free_host_, "cudaFreeHost", ptr);
			}

			/// \brief Free GPU memory via dynamically loaded cudaFreeAsync
			/// 
			/// When memory was allocated with malloc_async, this function must use the same stream as was used for 
			/// allocation!
			/// 
			/// \throw std::runtime_error if freeing fails
			void free_async(void* ptr, cudaStream_t stream = cudaStreamPerThread)
			{
				cuda_call(cuda_free_async_, "cudaFreeAsync", ptr, stream);
			}

			void memcpy(void* dst, const void* src, size_t count, cudaMemcpyKind kind)
			{
				cuda_call(cuda_memcpy_, "cudaMemcpy", dst, src, count, kind);
			}

			void memcpy_async(
				void* dst, 
				const void* src, 
				size_t count, 
				cudaMemcpyKind kind, 
				cudaStream_t stream = cudaStreamPerThread
			)
			{
				cuda_call(cuda_memcpy_async_, "cudaMemcpyAsync", dst, src, count, kind, stream);
			}

			/// \brief Set the maximum memory pool size for the given device to avoid e.g. stream synchronization freeing 
			///		   the whole pool
			void set_mem_pool_size(int device, uint64_t threshold = std::numeric_limits<uint64_t>::max())
			{
				cudaMemPool_t mempool{};
				cuda_call(cuda_device_get_default_mem_pool_, "cudaDeviceGetDefaultMemPool", &mempool, device);
				cuda_call(cuda_mempool_set_attribute_, "cudaMemPoolSetAttribute", cudaMemPoolAttrReleaseThreshold, &threshold);
			}

			/// \brief Returns a human-readable string for the given CUDA error code
			/// \param err The CUDA error code
			/// \return std::string containing the error message
			std::string get_error_str(cudaError_t err) const
			{
				// Calls the dynamically loaded cudaGetErrorString function
				const char* msg = cuda_call(cuda_get_error_str_, "cudaGetErrorString", err);
				return msg;
			}

			// Non-copyable and non-movable
			cuda_api(const cuda_api&) = delete;
			cuda_api& operator=(const cuda_api&) = delete;
			cuda_api(cuda_api&&) = delete;
			cuda_api& operator=(cuda_api&&) = delete;

		private:

			/// \brief Private constructor loads the CUDA library and hooks functions
			cuda_api()
			{

#if defined(_WIN32)
				std::string cuda_name = "cuda.dll";
#elif defined(__linux__)
				std::string cuda_name = "libcuda.so";
#else
				std::string cuda_name;
#endif

				handle_ = proc::load_library(cuda_name);

				cuda_malloc_ = proc::get_symbol<cuda_malloc_t>(handle_, "cudaMalloc", cuda_name);
				cuda_malloc_host_ = proc::get_symbol<cuda_malloc_host_t>(handle_, "cudaMallocHost", cuda_name);
				cuda_malloc_async_ = proc::get_symbol<cuda_malloc_async_t>(handle_, "cudaMallocAsync", cuda_name);
				cuda_free_ = proc::get_symbol<cuda_free_t>(handle_, "cudaFree", cuda_name);
				cuda_free_host_ = proc::get_symbol<cuda_free_host_t>(handle_, "cudaFreeHost", cuda_name);
				cuda_free_async_ = proc::get_symbol<cuda_free_async_t>(handle_, "cudaFreeAsync", cuda_name);

				cuda_memcpy_ = proc::get_symbol<cuda_memcpy_t>(handle_, "cudaMemcpy", cuda_name);
				cuda_memcpy_async_ = proc::get_symbol<cuda_memcpy_async_t>(handle_, "cudaMemcpy", cuda_name);

				cuda_device_get_default_mem_pool_ = proc::get_symbol<cuda_device_get_default_mem_pool_t>(
						handle_, 
						"cudaDeviceGetDefaultMemPool", 
						cuda_name
					);
				cuda_mempool_set_attribute_ = proc::get_symbol<cuda_mempool_set_attribute_t>(
					handle_,
					"cudaMemPoolSetAttribute",
					cuda_name
				);

				cuda_get_error_str_ = proc::get_symbol<cuda_get_error_str_t>(handle_, "cudaGetErrorString", cuda_name);
			}

			/// \brief Wrapper for calling dynamically loaded CUDA functions and checking error codes
			template<typename Func, typename... Args>
			static void cuda_call(Func func, std::string_view func_name, Args&&... args)
			{
				cudaError_t err = func(std::forward<Args>(args)...);
				if (err != cudaSuccess)
				{
					auto& inst = instance();
					const char* msg = inst.cuda_get_error_str_ ? inst.cuda_get_error_str_(err) : "unknown error";
					throw std::runtime_error(std::format("{} failed: {}", func_name, msg));
				}
			}

			// Cuda library handle, automatically freed/unloaded on destruction.
			proc::library_handle handle_ = nullptr;
			
			// Cuda function pointers dynamically loaded.
			cuda_malloc_t cuda_malloc_ = nullptr;
			cuda_malloc_host_t cuda_malloc_host_ = nullptr;
			cuda_malloc_async_t cuda_malloc_async_ = nullptr;
			cuda_free_t cuda_free_ = nullptr;
			cuda_free_host_t cuda_free_host_ = nullptr;
			cuda_free_async_t cuda_free_async_ = nullptr;

			cuda_memcpy_t cuda_memcpy_ = nullptr;
			cuda_memcpy_async_t cuda_memcpy_async_ = nullptr;

			cuda_device_get_default_mem_pool_t cuda_device_get_default_mem_pool_ = nullptr;
			cuda_mempool_set_attribute_t cuda_mempool_set_attribute_ = nullptr;

			cuda_get_error_str_t cuda_get_error_str_ = nullptr;

		};

	} // namespace cuda

} // namespace NAMESPACE_COMPRESSED_IMAGE