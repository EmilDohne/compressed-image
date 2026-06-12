#pragma once

#include <vector>

#include "compressed/macros.h"
#include "compressed/cuda/memory.h"
#include "compressed/cuda/filters/enums.h"
#include "compressed/cuda/filters/struct.h"
#include "compressed/cuda/filters/bytedelta.h"
#include "compressed/cuda/filters/delta.h"
#include "compressed/cuda/filters/xordelta.h"
#include "compressed/cuda/filters/shuffle.h"
#include "compressed/cuda/filters/fmap.h"

namespace
NAMESPACE_COMPRESSED_IMAGE
{
    namespace cuda
    {
        namespace detail
        {
            [[nodiscard]] inline bool apply_fwd_filter(const gpu_filter& filter,
                                                       const uint8_t* input,
                                                       uint8_t* output,
                                                       const size_t length,
                                                       const size_t type_size)
            {
                if (filter.type == cuda::enums::filter::bytedelta)
                {
                    if (auto& inst = filter::bytedelta::instance(); !inst.available())
                    {
                        get_logger()->warn(
                            "CUDA bytedelta filter unavailable, likely because of a missing shared library."
                            " Skipping this filter."
                        );
                        return false;
                    }
                    filter::bytedelta::instance().forward(input, output, length, type_size);
                }
                else if (filter.type == cuda::enums::filter::shuffle)
                {
                    if (auto& inst = filter::shuffle::instance(); !inst.available())
                    {
                        get_logger()->warn(
                            "CUDA shuffle filter unavailable, likely because of a missing shared library."
                            " Skipping this filter."
                        );
                        return false;
                    }
                    filter::shuffle::instance().forward(input, output, length, type_size);
                }
                else if (filter.type == cuda::enums::filter::delta)
                {
                    if (auto& inst = filter::delta::instance(); !inst.available())
                    {
                        get_logger()->warn(
                            "CUDA delta filter unavailable, likely because of a missing shared library."
                            " Skipping this filter."
                        );
                        return false;
                    }
                    filter::delta::instance().forward(input, output, length, type_size);
                }
                else if (filter.type == cuda::enums::filter::xordelta)
                {
                    if (auto& inst = filter::xordelta::instance(); !inst.available())
                    {
                        get_logger()->warn(
                            "CUDA xordelta filter unavailable, likely because of a missing shared library."
                            " Skipping this filter."
                        );
                        return false;
                    }
                    filter::xordelta::instance().forward(input, output, length, type_size);
                }
                else if (filter.type == cuda::enums::filter::fmap)
                {
                    if (auto& inst = filter::fmap::instance(); !inst.available())
                    {
                        get_logger()->warn(
                            "CUDA fmap filter unavailable, likely because of a missing shared library."
                            " Skipping this filter."
                        );
                        return false;
                    }
                    filter::fmap::instance().forward(input, output, length, type_size);
                }
                else
                {
                    get_logger()->critical(std::format("Unknown filter type: {}", enums::to_string(filter.type)));
                    return false;
                }

                return true;
            }

            [[nodiscard]] inline bool apply_bwd_filter(const gpu_filter& filter,
                                                       const uint8_t* input,
                                                       uint8_t* output,
                                                       const size_t length,
                                                       const size_t type_size)
            {
                if (filter.type == cuda::enums::filter::bytedelta)
                {
                    if (auto& inst = filter::bytedelta::instance(); !inst.available())
                    {
                        get_logger()->warn(
                            "CUDA bytedelta filter unavailable, likely because of a missing shared library."
                            " Skipping this filter in backward pipeline."
                        );
                        return false;
                    }
                    filter::bytedelta::instance().backward(input, output, length, type_size);
                }
                else if (filter.type == cuda::enums::filter::shuffle)
                {
                    if (auto& inst = filter::shuffle::instance(); !inst.available())
                    {
                        get_logger()->warn(
                            "CUDA shuffle filter unavailable, likely because of a missing shared library."
                            " Skipping this filter in backward pipeline."
                        );
                        return false;
                    }
                    filter::shuffle::instance().backward(input, output, length, type_size);
                }
                else if (filter.type == cuda::enums::filter::delta)
                {
                    if (auto& inst = filter::delta::instance(); !inst.available())
                    {
                        get_logger()->warn(
                            "CUDA delta filter unavailable, likely because of a missing shared library."
                            " Skipping this filter in backward pipeline."
                        );
                        return false;
                    }
                    filter::delta::instance().backward(input, output, length, type_size);
                }
                else if (filter.type == cuda::enums::filter::xordelta)
                {
                    if (auto& inst = filter::xordelta::instance(); !inst.available())
                    {
                        get_logger()->warn(
                            "CUDA xordelta filter unavailable, likely because of a missing shared library."
                            " Skipping this filter in backward pipeline."
                        );
                        return false;
                    }
                    filter::xordelta::instance().backward(input, output, length, type_size);
                }
                else if (filter.type == cuda::enums::filter::fmap)
                {
                    if (auto& inst = filter::fmap::instance(); !inst.available())
                    {
                        get_logger()->warn(
                            "CUDA fmap filter unavailable, likely because of a missing shared library."
                            " Skipping this filter in backward pipeline."
                        );
                        return false;
                    }
                    filter::fmap::instance().backward(input, output, length, type_size);
                }
                else
                {
                    get_logger()->critical(std::format("Unknown filter type: {}", enums::to_string(filter.type)));
                    return false;
                }

                return true;
            }
        }


        template <typename T>
        void apply_forward_pipeline(std::vector<gpu_filter>& filters, const void* input, void* output, size_t length)
        {
            _COMPRESSED_PROFILE_FUNCTION();

            // ##################################################################################
            // No filters -> passthrough
            // ##################################################################################
            if (filters.empty())
            {
                // We account for this case here by memcpying, but callers should avoid
                // invoking this function if no filters are present.
                cuda_api::instance().memcpy_async(
                    output,
                    input,
                    length,
                    cudaMemcpyDeviceToDevice
                );
                return;
            }

            // ##################################################################################
            // Single filter
            // ##################################################################################

            // Apply the forward pipeline on it, on failure, do passthrough
            // (errors are logged by the apply_fwd_filter function).
            if (filters.size() == 1)
            {
                const auto success = detail::apply_fwd_filter(
                    filters[0],
                    static_cast<const uint8_t*>(input),
                    static_cast<uint8_t*>(output),
                    length,
                    sizeof(T)
                );

                // filter
                if (!success)
                {
                    cuda_api::instance().memcpy_async(
                        output,
                        input,
                        length,
                        cudaMemcpyDeviceToDevice
                    );
                }
                return;
            }


            // ##################################################################################
            // Multiple filters
            // ##################################################################################

            // ping-pong the `output` and `temp_buffer` back and forth to avoid reallocating for every filter.
            auto temp_buffer = make_device_buffer_async<uint8_t>(length);

            const void* current_src = input;
            void* current_dst = output; // Try to land exactly on output first

            for (const auto& filter : filters)
            {
                const bool success = detail::apply_fwd_filter(
                    filter,
                    static_cast<const uint8_t*>(current_src),
                    static_cast<uint8_t*>(current_dst),
                    length,
                    sizeof(T)
                );

                if (success)
                {
                    // Advance pipeline only if successful
                    current_src = current_dst;

                    // Swap destination: if we just wrote to output, route the next pass to temp
                    current_dst = (current_dst == output) ? static_cast<void*>(temp_buffer.get()) : output;
                }
            }

            // Resolve buffers.
            if (current_src == input)
            {
                // ALL filters failed/skipped. Perform passthrough.
                cuda_api::instance().memcpy_async(output, input, length, cudaMemcpyDeviceToDevice);
            }
            else if (current_src != output)
            {
                // The final successful filter landed in temp_buffer. Copy it to actual output.
                cuda_api::instance().memcpy_async(output, current_src, length, cudaMemcpyDeviceToDevice);
            }
        }


        template <typename T>
        void apply_backward_pipeline(const std::vector<gpu_filter>& filters,
                                     const void* input,
                                     void* output,
                                     size_t length)
        {
            _COMPRESSED_PROFILE_FUNCTION();

            // ##################################################################################
            // No filters -> passthrough
            // ##################################################################################
            if (filters.empty())
            {
                cuda_api::instance().memcpy_async(output, input, length, cudaMemcpyDeviceToDevice);
                return;
            }

            // ##################################################################################
            // Single filter
            // ##################################################################################
            if (filters.size() == 1)
            {
                const bool success = detail::apply_bwd_filter(
                    filters[0],
                    static_cast<const uint8_t*>(input),
                    static_cast<uint8_t*>(output),
                    length,
                    sizeof(T)
                );

                if (!success)
                {
                    cuda_api::instance().memcpy_async(output, input, length, cudaMemcpyDeviceToDevice);
                }
                return;
            }

            // ##################################################################################
            // Single filter
            // ##################################################################################

            // ping-pong the `output` and `temp_buffer` back and forth to avoid reallocating for every filter.
            auto temp_buffer = make_device_buffer_async<uint8_t>(length);

            const void* current_src = input;
            void* current_dst = output;

            // Iterate in reverse for the backward pipeline.
            for (auto it = filters.rbegin(); it != filters.rend(); ++it)
            {
                const bool success = detail::apply_bwd_filter(
                    *it,
                    static_cast<const uint8_t*>(current_src),
                    static_cast<uint8_t*>(current_dst),
                    length,
                    sizeof(T)
                );

                if (success)
                {
                    current_src = current_dst;
                    current_dst = (current_dst == output) ? static_cast<void*>(temp_buffer.get()) : output;
                }
            }

            // Resolve buffers.
            if (current_src == input)
            {
                cuda_api::instance().memcpy_async(output, input, length, cudaMemcpyDeviceToDevice);
            }
            else if (current_src != output)
            {
                cuda_api::instance().memcpy_async(output, current_src, length, cudaMemcpyDeviceToDevice);
            }
        }
    }
}

