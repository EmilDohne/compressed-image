#include <cuda_runtime.h>
#include <stdint.h>

// ##########################################################################
// fmap Kernels (compressed_fmap_plugin)
//
// Monotonic order map for IEEE-754 bit patterns. IEEE floats are
// sign-magnitude: as raw integers, negative values sort above positive ones
// and in *reverse* order among themselves, so arithmetic deltas across sign
// or magnitude changes produce huge residuals.
//
// This bijection remaps the bits so that unsigned integer ordering matches
// numeric ordering (the same trick used to radix-sort floats):
//
//   forward:  sign set  -> flip ALL bits   (un-reverses the negatives)
//             sign clear-> flip sign bit   (moves positives above negatives)
//   backward: exact inverse
//
// After the map, x < y numerically  <=>  map(x) < map(y) as unsigned ints,
// so a subsequent (wrapped) integer delta produces small, magnitude-ordered
// residuals on smooth data -- exactly what zigzag + shuffle exploit.
//
// Properties:
//   - Bijection over ALL bit patterns: NaN payloads, +/-Inf, -0.0 vs +0.0
//     and denormals round-trip bit-exactly. Strictly lossless.
//   - Element-wise and branch-cheap; safe in-place or out-of-place.
//   - type_size 2 = half, 4 = float, 8 = double. (1 is rejected: there is
//     no 8-bit float type in this pipeline.)
//
// Intended pipeline position: FIRST, before delta:
//   forward:  fmap -> delta(+zigzag) -> shuffle -> compressor
//   backward: un-shuffle -> un-delta -> un-fmap
// (The backward pipeline iterates filters in reverse, so just list it first.)
// ##########################################################################

namespace
{
    template <typename T>
    __device__ __forceinline__ T sign_mask()
    {
        return static_cast<T>(static_cast<T>(1) << (sizeof(T) * 8 - 1));
    }

    template <typename T>
    __global__ void fmap_forward_kernel(const T* __restrict__ input,
                                        T* __restrict__ output,
                                        size_t num_elements)
    {
        const size_t tid = static_cast<size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
        if (tid < num_elements)
        {
            const T v    = input[tid];
            const T sign = sign_mask<T>();

            // negative: flip all bits; positive: flip just the sign bit
            output[tid] = (v & sign) ? static_cast<T>(~v)
                                     : static_cast<T>(v | sign);
        }
    }

    template <typename T>
    __global__ void fmap_backward_kernel(const T* __restrict__ input,
                                         T* __restrict__ output,
                                         size_t num_elements)
    {
        const size_t tid = static_cast<size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
        if (tid < num_elements)
        {
            const T v    = input[tid];
            const T sign = sign_mask<T>();

            // mapped positives have the sign bit set, mapped negatives don't
            output[tid] = (v & sign) ? static_cast<T>(v & static_cast<T>(~sign))
                                     : static_cast<T>(~v);
        }
    }

    template <typename T>
    cudaError_t launch_fmap(const uint8_t* d_input,
                            uint8_t* d_output,
                            size_t num_elements,
                            bool forward,
                            cudaStream_t stream)
    {
        constexpr int threads = 256;
        const int blocks = static_cast<int>((num_elements + threads - 1) / threads);

        if (forward)
        {
            fmap_forward_kernel<T><<<blocks, threads, 0, stream>>>(
                reinterpret_cast<const T*>(d_input),
                reinterpret_cast<T*>(d_output),
                num_elements
            );
        }
        else
        {
            fmap_backward_kernel<T><<<blocks, threads, 0, stream>>>(
                reinterpret_cast<const T*>(d_input),
                reinterpret_cast<T*>(d_output),
                num_elements
            );
        }
        return cudaGetLastError();
    }

    cudaError_t run_fmap(const uint8_t* d_input,
                         uint8_t* d_output,
                         size_t length_bytes,
                         size_t type_size,
                         bool forward,
                         cudaStream_t stream)
    {
        if (length_bytes == 0 || type_size == 0) return cudaErrorInvalidValue;

        const size_t num_elements = length_bytes / type_size;

        switch (type_size)
        {
        case 2:  return launch_fmap<uint16_t>(d_input, d_output, num_elements, forward, stream); // half
        case 4:  return launch_fmap<uint32_t>(d_input, d_output, num_elements, forward, stream); // float
        case 8:  return launch_fmap<uint64_t>(d_input, d_output, num_elements, forward, stream); // double
        default: return cudaErrorInvalidValue; // no 8-bit float type
        }
    }
}

// ##########################################################################
// C-/C++-API exports
// ##########################################################################

#if defined(_WIN32)
#define PLUGIN_EXPORT __declspec(dllexport)
#else
#define PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {

PLUGIN_EXPORT cudaError_t run_fmap_forward(
    const uint8_t* d_input,
    uint8_t* d_output,
    size_t length_bytes,
    size_t type_size,
    cudaStream_t stream)
{
    return run_fmap(d_input, d_output, length_bytes, type_size, /*forward=*/true, stream);
}

PLUGIN_EXPORT cudaError_t run_fmap_backward(
    const uint8_t* d_input,
    uint8_t* d_output,
    size_t length_bytes,
    size_t type_size,
    cudaStream_t stream)
{
    return run_fmap(d_input, d_output, length_bytes, type_size, /*forward=*/false, stream);
}

} // extern "C"
