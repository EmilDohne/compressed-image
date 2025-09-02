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

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "compressed/macros.h"
#include "compressed/cuda/exceptions.h"


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

			using cuda_malloc_t			= cudaError_t(*)(void**, size_t);
			using cuda_free_t			= cudaError_t(*)(void*);
			using cuda_get_error_str_t	= const char* (*)(cudaError_t);

			/// \brief Access the singleton instance
			/// \return Reference to the cuda_api singleton
			static cuda_api& instance()
			{
				static cuda_api instance;
				return instance;
			}

			/// \brief Allocate memory on GPU via dynamically loaded cudaMalloc
			/// \throw std::runtime_error if allocation fails
			void malloc(void** ptr, size_t size) const
			{
				cuda_call(cuda_malloc_, ptr, size, "cudaMalloc");
			}

			/// \brief Free GPU memory via dynamically loaded cudaFree
			/// \throw std::runtime_error if freeing fails
			void free(void* ptr) const
			{
				cuda_call(cuda_free_, ptr, "cudaFree");
			}

			/// \brief Returns a human-readable string for the given CUDA error code
			/// \param err The CUDA error code
			/// \return std::string containing the error message
			std::string get_error_str(cudaError_t err) const
			{
				// Calls the dynamically loaded cudaGetErrorString function
				const char* msg = cuda_call(cuda_get_error_str_, err, "cudaGetErrorString");
				return std::string(msg);
			}

			/// \brief Accessors for the raw function pointers if needed
			[[nodiscard]] cuda_malloc_t cuda_malloc() const { return cuda_malloc_; }
			[[nodiscard]] cuda_free_t cuda_free() const { return cuda_free_; }
			[[nodiscard]] cuda_get_error_str_t cuda_get_error_string() const { return cuda_get_error_str_; }

			// Non-copyable and non-movable
			cuda_api(const cuda_api&) = delete;
			cuda_api& operator=(const cuda_api&) = delete;
			cuda_api(cuda_api&&) = delete;
			cuda_api& operator=(cuda_api&&) = delete;

		private:

			/// \brief Private constructor loads the CUDA library and hooks functions
			cuda_api()
			{

				// Cuda is for all intents and purposes only supported on windows/linux. Therefore we don't support
				// loading from anything besides those two.
#if defined(_WIN32)
				handle_ = LoadLibraryA("cuda.dll");
				if (!handle_) throw library_not_found("CUDA DLL not found");
				cuda_malloc_ = reinterpret_cast<cuda_malloc_t>(GetProcAddress((HMODULE)handle_, "cudaMalloc"));
				cuda_free_ = reinterpret_cast<cuda_free_t>(GetProcAddress((HMODULE)handle_, "cudaFree"));
				cuda_get_error_str_ = reinterpret_cast<cuda_get_error_str_t>(GetProcAddress((HMODULE)handle_, "cudaGetErrorString"));
#elif defined(__linux__)
				handle_ = dlopen("libcuda.so", RTLD_LAZY);
				if (!handle_) throw library_not_found("CUDA library not found");
				cuda_malloc_ = reinterpret_cast<cuda_malloc_t>(dlsym(handle_, "cudaMalloc"));
				cuda_free_ = reinterpret_cast<cuda_free_t>(dlsym(handle_, "cudaFree"));
				cuda_get_error_str_ = reinterpret_cast<cuda_get_error_str_t>(dlsym(handle_, "cudaGetErrorString"));
#else
				throw library_not_found("CUDA library not found, this is likely due to an unsupported platform.");
#endif

				if (!cuda_malloc_) throw function_not_found("Failed to find function cudaMalloc");
				if (!cuda_free_)   throw function_not_found("Failed to find function cudaFree");
				if (!cuda_get_error_str_)   throw function_not_found("Failed to find function cudaGetErrorString");
			}

			/// \brief Destructor closes the CUDA library handle
			~cuda_api()
			{
#if defined(_WIN32)
				if (handle_) FreeLibrary((HMODULE)handle_);
#elif defined(__linux__)
				if (handle_) dlclose(handle_);
#endif
			}

			/// \brief Wrapper for calling dynamically loaded CUDA functions and checking error codes
			template<typename Func, typename... Args>
			static void cuda_call(Func func, Args&&... args, std::string_view func_name)
			{
				cudaError_t err = func(std::forward<Args>(args)...);
				if (err != cudaSuccess)
				{
					auto& inst = instance();
					const char* msg = inst.cuda_get_error_str_ ? inst.cuda_get_error_str_(err) : "unknown error";
					throw std::runtime_error(std::format("{} failed: {}", func_name, msg));
				}
			}

			// Cuda library handle
			void* handle_ = nullptr;
			
			// Cuda function pointers dynamically loaded.
			cuda_malloc_t cuda_malloc_ = nullptr;
			cuda_free_t cuda_free_ = nullptr;
			cuda_get_error_str_t cuda_get_error_str_ = nullptr;

		};

	} // namespace cuda

} // namespace NAMESPACE_COMPRESSED_IMAGE