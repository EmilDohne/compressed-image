#include <cuda_runtime.h>
#include <cub/cub.cuh>
#include <stdint.h>

// ##########################################################################
// Regular Delta Kernels (Typed Subtraction / Addition)
// ##########################################################################
//
// Flat 1D horizontal delta (residual against the left neighbour). This is the proven default.
// `row_stride` is accepted only for ABI parity with the other filters and is ignored here.
//
// 16-bit data is additionally zigzag-encoded (signed residual -> small unsigned), a near-free
// cratio win.

template <typename T> // T is the unsigned type
__device__ __forceinline__ T zigzag_encode(T v)
{
    using S = typename std::make_signed<T>::type;
    S s = static_cast<S>(v);
    constexpr int shift = sizeof(T) * 8 - 1;
    return (static_cast<T>(s) << 1) ^ static_cast<T>(s >> shift); // arithmetic shift
}

template <typename T>
__device__ __forceinline__ T zigzag_decode(T u)
{
    return (u >> 1) ^ (~(u & 1) + 1); // (u >> 1) ^ -(u & 1)
}

template <typename T>
__global__ void delta_forward_kernel(const T* input, T* output, const size_t num_elements)
{
    size_t tid = blockIdx.x * blockDim.x + threadIdx.x;

    if (tid < num_elements)
    {
        if (tid == 0)
        {
            if constexpr (sizeof(T) == 2)
            {
                output[tid] = zigzag_encode(input[tid]);
            }
            else
            {
                output[tid] = input[tid];
            }
        }
        else
        {
            if constexpr (sizeof(T) == 2)
            {
                output[tid] = zigzag_encode(static_cast<T>(input[tid] - input[tid - 1]));
            }
            else
            {
                output[tid] = input[tid] - input[tid - 1];
            }
        }
    }
}

template <typename T, int BLOCK_THREADS>
__global__ void delta_backward_kernel(const T* input, T* output, size_t num_elements)
{
    typedef cub::BlockScan<T, BLOCK_THREADS> BlockScan;
    __shared__ typename BlockScan::TempStorage temp_storage;

    T carry = 0;

    for (size_t i_base = 0; i_base < num_elements; i_base += BLOCK_THREADS)
    {
        size_t i = i_base + threadIdx.x;
        bool valid = (i < num_elements);

        T thread_data{};
        if constexpr (sizeof(T) == 2)
        {
            thread_data = valid ? zigzag_decode(input[i]) : 0;
        }
        else
        {
            thread_data = valid ? input[i] : 0;
        }
        T block_sum;
        BlockScan(temp_storage).InclusiveSum(thread_data, thread_data, block_sum);

        if (valid)
        {
            output[i] = thread_data + carry;
        }

        carry += block_sum;

        __syncthreads();
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

namespace
{
template <typename T>
void launch_delta_forward(const uint8_t* d_in, uint8_t* d_out, size_t num_elements, cudaStream_t stream)
{
    const auto* in = reinterpret_cast<const T*>(d_in);
    auto* out = reinterpret_cast<T*>(d_out);
    const int threads = 256;
    size_t blocks = (num_elements + threads - 1) / threads;
    delta_forward_kernel<T><<<blocks, threads, 0, stream>>>(in, out, num_elements);
}

template <typename T>
void launch_delta_backward(const uint8_t* d_in, uint8_t* d_out, size_t num_elements, cudaStream_t stream)
{
    const auto* in = reinterpret_cast<const T*>(d_in);
    auto* out = reinterpret_cast<T*>(d_out);
    const int block_threads = 256;
    delta_backward_kernel<T, block_threads><<<1, block_threads, 0, stream>>>(in, out, num_elements);
}
}

extern "C" {
PLUGIN_EXPORT cudaError_t run_delta_forward(
    const uint8_t* d_input,
    uint8_t* d_output,
    size_t length_bytes,
    size_t type_size,
    size_t row_stride,
    cudaStream_t stream)
{
    (void)row_stride; // accepted for ABI parity; this 1D filter does not use it.
    if (length_bytes == 0 || type_size == 0) return cudaErrorInvalidValue;

    size_t num_elements = length_bytes / type_size;

    switch (type_size)
    {
    case 1: launch_delta_forward<uint8_t>(d_input, d_output, num_elements, stream); break;
    case 2: launch_delta_forward<uint16_t>(d_input, d_output, num_elements, stream); break;
    case 4: launch_delta_forward<uint32_t>(d_input, d_output, num_elements, stream); break;
    case 8: launch_delta_forward<uint64_t>(d_input, d_output, num_elements, stream); break;
    default: return cudaErrorInvalidValue;
    }
    return cudaGetLastError();
}

PLUGIN_EXPORT cudaError_t run_delta_backward(
    const uint8_t* d_input,
    uint8_t* d_output,
    const size_t length_bytes,
    const size_t type_size,
    size_t row_stride,
    cudaStream_t stream)
{
    (void)row_stride;
    if (length_bytes == 0 || type_size == 0) return cudaErrorInvalidValue;

    size_t num_elements = length_bytes / type_size;

    switch (type_size)
    {
    case 1: launch_delta_backward<uint8_t>(d_input, d_output, num_elements, stream); break;
    case 2: launch_delta_backward<uint16_t>(d_input, d_output, num_elements, stream); break;
    case 4: launch_delta_backward<uint32_t>(d_input, d_output, num_elements, stream); break;
    case 8: launch_delta_backward<uint64_t>(d_input, d_output, num_elements, stream); break;
    default: return cudaErrorInvalidValue;
    }
    return cudaGetLastError();
}
}
