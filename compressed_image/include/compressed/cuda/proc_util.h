/*
Header for various procutils such as finding symbols in a file and raising the appropriate error if it cannot be located.

Note: This expects windows/linux as it is part of the cuda subfolder which only support windows and linux. It is not 
	  intended to be a generic dll/so module. It is an implementation detail of compressed-image and should not be used
	  outside of it!
*/

#pragma once

#include <format>
#include <memory>

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

		namespace proc
		{

			// Platform-independent handle type
#if defined(_WIN32)
			// HMODULE decays down to PVOID which is void* but for ease of use later we alias it directly.
			using library_handle_impl_ = HMODULE;
#else
			using library_handle_impl_ = void*;
#endif

			/// Custom deleter for unique_ptr freeing/closing a dll/so automatically on destruction.
			struct library_deleter 
			{
				void operator()(library_handle_impl_ handle) const
				{
					if (!handle) return;
#if defined(_WIN32)
					FreeLibrary(handle);
#else
					dlclose(handle);
#endif
				}
			};

			/// Unique-ptr wrapped handle pointer, is automatically freed on destruction.
			using library_handle = std::unique_ptr<std::remove_pointer_t<library_handle_impl_>, library_deleter>;

			// Function to load a library and return a unique_ptr-managed handle
			inline library_handle load_library(std::string name)
			{
				library_handle_impl_ handle = nullptr;

#if defined(_WIN32)
				handle = LoadLibraryA(name.c_str());
#elif defined(__linux__)
				handle = dlopen(name.c_str(), RTLD_GLOBAL | RTLD_LAZY);
#endif

				if (!handle)
				{
					throw library_not_found(std::format("Failed to load library: {}", name));
				}

				return library_handle(handle);
			}

			/// \brief retrieves the symbol `symbol_name` from the given library handle
			///
			/// \param handle The library handle to load the symbol from
			/// \param symbol_name The symbol name to load
			/// \param object_name The name of the library handle. may be left empty, only used for error messages.
			template <typename func_sig>
			func_sig get_symbol(library_handle& handle, std::string symbol_name, std::string object_name)
			{
				if (!handle)
				{
					throw std::invalid_argument(
						std::format(
							"Internal: passed empty library handle while retrieving symbol of name {} from object {}",
							symbol_name,
							object_name
						)
					);
				}

#if defined(_WIN32)
				func_sig func_ptr = reinterpret_cast<func_sig>(GetProcAddress(handle.get(), symbol_name.c_str()));
#elif defined(__linux__)
				func_sig func_ptr = reinterpret_cast<func_sig>(dlsym(handle.get(), symbol_name.c_str()));
#else
				func_sig func_ptr = nullptr;
				throw symbol_not_found(
					std::format(
						"Unable to find symbol {} in library {} as we are on an unsupported platform for CUDA.",
						symbol_name, object_name
					)
				);
#endif

				if (!func_ptr)
				{
					throw symbol_not_found(
						std::format(
							"Unable to find symbol {} in library {} while dynamically loading it.",
							symbol_name, object_name
						)
					);
				}
				return func_ptr;
			}

		} // namespace proc

	} // namespace cuda


} // namespace NAMESPACE_COMPRESSED_IMAGE