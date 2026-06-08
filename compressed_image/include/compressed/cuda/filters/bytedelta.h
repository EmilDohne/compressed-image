#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <cuda_runtime.h>

#include "compressed/logger.h"
#include "compressed/macros.h"
#include "compressed/detail/scoped_timer.h"
#include "compressed/cuda/proc_util.h"
#include "compressed/cuda/cuda_hook.h"
#include "compressed/cuda/filters/util.h"
#include "compressed/cuda/filters/enums.h"


/// The bytedelta compiled plugin. We assume that this plugin lives somewhere on the search path and load it from there
/// Note:
///     If this is ever changed in the cmake, the plugin name must be updated here as well.
namespace
NAMESPACE_COMPRESSED_IMAGE::cuda::detail::bytedelta
{
#if defined(_WIN32)
    constexpr static std::string_view s_plugin_name = "compressed_bytedelta_plugin.dll";
#elif defined(__linux__)
    constexpr static std::string_view s_plugin_name = "libcompressed_bytedelta_plugin.so";
#else
    constexpr static std::string_view s_plugin_name;
#endif
}


namespace
NAMESPACE_COMPRESSED_IMAGE
{
    namespace cuda
    {
        namespace filter
        {
            /// \brief bytedelta filter plugin for CUDA compression.
            ///
            /// This filter is dynamically loaded and hooked as a plugin. It can only be loaded if the plugin `s_plugin_name`
            /// is found on the search path.
            ///
            /// It performs a simple delta encoding per-byte on the local chunks
            struct bytedelta
            {
                constexpr static cuda::enums::filter filter_type()
                {
                    return cuda::enums::filter::bytedelta;
                }

                static bytedelta& instance()
                {
                    static bytedelta inst;
                    return inst;
                }

                bool available() const noexcept
                {
                    return handle_ != nullptr;
                }

                void forward(
                    const uint8_t* d_input,
                    uint8_t* d_output,
                    size_t length,
                    size_t typesize,
                    cudaStream_t stream = cudaStreamPerThread) const;

                void backward(
                    const uint8_t* d_input,
                    uint8_t* d_output,
                    size_t length,
                    size_t typesize,
                    cudaStream_t stream = cudaStreamPerThread) const;

                /// Singleton, so no copy, move, etc.
                bytedelta(const bytedelta&) = delete;
                bytedelta& operator=(const bytedelta&) = delete;
                bytedelta(bytedelta&&) = delete;
                bytedelta& operator=(bytedelta&&) = delete;

            private:
                bytedelta();

                template <typename Func, typename... Args>
                static void plugin_call(Func func, std::string_view func_name, Args&&... args);

                proc::library_handle handle_ = nullptr;

                detail::filter_function_t forward_fn_ = nullptr;
                detail::filter_function_t backward_fn_ = nullptr;
            };

            // ##########################################################################
            // Implementation
            // ##########################################################################

            inline void bytedelta::forward(
                const uint8_t* d_input,
                uint8_t* d_output,
                size_t length,
                size_t typesize,
                cudaStream_t stream) const
            {
                _COMPRESSED_PROFILE_FUNCTION();
                plugin_call(forward_fn_, "run_bytedelta_forward", d_input, d_output, length, typesize, stream);
            }

            inline void bytedelta::backward(
                const uint8_t* d_input,
                uint8_t* d_output,
                size_t length,
                size_t typesize,
                cudaStream_t stream) const
            {
                _COMPRESSED_PROFILE_FUNCTION();
                plugin_call(backward_fn_, "run_bytedelta_backward", d_input, d_output, length, typesize, stream);
            }

            template <typename Func, typename... Args>
            void bytedelta::plugin_call(Func func, std::string_view func_name, Args&&... args)
            {
                if (!func)
                {
                    throw std::runtime_error(
                        std::format(
                            "Filter plugin function '{}' is unavailable (plugin '{}' not loaded).",
                            func_name,
                            detail::bytedelta::s_plugin_name
                        )
                    );
                }

                if (const cudaError_t err = func(std::forward<Args>(args)...); err != cudaSuccess)
                {
                    throw std::runtime_error(
                        std::format("{} failed: {}", func_name, cuda_api::instance().get_error_string(err))
                    );
                }
            }

            inline bytedelta::bytedelta()
            {
                handle_ = proc::load_library(std::string(detail::bytedelta::s_plugin_name));
                if (!handle_)
                {
                    return;
                }

                this->forward_fn_ = proc::get_symbol<decltype(this->forward_fn_)>(
                    handle_,
                    "run_bytedelta_forward",
                    std::string(detail::bytedelta::s_plugin_name)
                );
                this->backward_fn_ = proc::get_symbol<decltype(this->backward_fn_)>(
                    handle_,
                    "run_bytedelta_backward",
                    std::string(detail::bytedelta::s_plugin_name)
                );
            }
        }
    } // namespace cuda
} // namespace NAMESPACE_COMPRESSED_IMAGE
