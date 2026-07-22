#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "compressed/cuda/gpu.h"

namespace py = pybind11;


namespace compressed_py
{

    /// Bind free functions for querying CUDA/GPU availability. The `*_gpu` codecs fall back to a
    /// CPU-equivalent when CUDA is unavailable, so these let a caller find out whether the GPU path
    /// is actually being used.
    void bind_cuda(py::module_& m)
    {
        m.def("cuda_available", []()
        {
            return compressed::cuda::is_available();
        }, R"doc(
Check whether the CUDA runtime, the nvcomp library and at least one CUDA device are all available.

When this returns False, the `*_gpu` codecs still work but transparently fall back to their CPU
equivalents (see compressed_image.Codec), so this is the way to confirm the GPU path is active.

:returns: True if compression/decompression can run on the GPU, otherwise False.
        )doc");

        m.def("cuda_device_count", []()
        {
            return compressed::cuda::is_available() ? compressed::cuda::device_count() : 0;
        }, R"doc(
:returns: The number of available CUDA devices, or 0 if CUDA is unavailable.
        )doc");

        m.def("cuda_device_names", []() -> std::vector<std::string>
        {
            if (!compressed::cuda::is_available())
            {
                return {};
            }
            return compressed::cuda::device_names();
        }, R"doc(
:returns: The names of all available CUDA devices, or an empty list if CUDA is unavailable.
        )doc");

        m.def("cuda_current_device", []()
        {
            return compressed::cuda::is_available() ? compressed::cuda::current_device() : -1;
        }, R"doc(
:returns: The index of the currently active CUDA device, or -1 if CUDA is unavailable.
        )doc");
    }

} // compressed_py
