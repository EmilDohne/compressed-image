#include <cuda_runtime.h>
#include <cub/cub.cuh>
#include <stdint.h>

// ##########################################################################
// XOR Delta Kernels (Bitwise XOR)
// ##########################################################################
//
// Flat 1D horizontal XOR delta (against the left neighbour). `row_stride` is accepted only for
// ABI parity and is ignored.

/// \brief Functor for computing the bitwise XOR of two elements in CUB scans
struct XorOp
{
    template <typename T>
    __device__ __forceinline__ T operator()(const T& a, const T& b) const
    {
        return a ^ b;
    }
};

template <typename T>
__global__ void xor_delta_forward_kernel(const T* input, T* output, size_t num_elements)
{
    size_t tid = blockIdx.x * blockDim.x + threadIdx.x;

    if (tid < num_elements)
    {
        if (tid == 0)
        {
            output[tid] = input[tid];
        }
        else
        {
            output[tid] = input[tid] ^ input[tid - 1];
        }
    }
}

template <typename T, int BLOCK_THREADS>
__global__ void xor_delta_backward_kernel(const T* input, T* output, size_t num_elements)
{
    typedef cub::BlockScan<T, BLOCK_THREADS> BlockScan;
    __shared__ typename BlockScan::TempStorage temp_storage;

    T carry = 0;

    for (size_t i_base = 0; i_base < num_elements; i_base += BLOCK_THREADS)
    {
        size_t i = i_base + threadIdx.x;
        bool valid = (i < num_elements);

        T thread_data = valid ? input[i] : 0;
        T block_sum;

        // XOR is its own inverse; we use a custom XorOp for the prefix scan
        BlockScan(temp_storage).InclusiveScan(thread_data, thread_data, XorOp(), block_sum);

        if (valid)
        {
            output[i] = thread_data ^ carry;
        }

        carry ^= block_sum;

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
void launch_xor_forward(const uint8_t* d_in, uint8_t* d_out, size_t num_elements, cudaStream_t stream)
{
    const auto* in = reinterpret_cast<const T*>(d_in);
    auto* out = reinterpret_cast<T*>(d_out);
    const int threads = 256;
    size_t blocks = (num_elements + threads - 1) / threads;
    xor_delta_forward_kernel<T><<<blocks, threads, 0, stream>>>(in, out, num_elements);
}

template <typename T>
void launch_xor_backward(const uint8_t* d_in, uint8_t* d_out, size_t num_elements, cudaStream_t stream)
{
    const auto* in = reinterpret_cast<const T*>(d_in);
    auto* out = reinterpret_cast<T*>(d_out);
    const int block_threads = 256;
    xor_delta_backward_kernel<T, block_threads><<<1, block_threads, 0, stream>>>(in, out, num_elements);
}
}

extern "C" {
PLUGIN_EXPORT cudaError_t run_xor_delta_forward(
    const uint8_t* d_input,
    uint8_t* d_output,
    size_t length_bytes,
    size_t type_size,
    size_t row_stride,
    cudaStream_t stream)
{
    (void)row_stride;
    if (length_bytes == 0 || type_size == 0) return cudaErrorInvalidValue;

    size_t num_elements = length_bytes / type_size;

    switch (type_size)
    {
    case 1: launch_xor_forward<uint8_t>(d_input, d_output, num_elements, stream); break;
    case 2: launch_xor_forward<uint16_t>(d_input, d_output, num_elements, stream); break;
    case 4: launch_xor_forward<uint32_t>(d_input, d_output, num_elements, stream); break;
    case 8: launch_xor_forward<uint64_t>(d_input, d_output, num_elements, stream); break;
    default: return cudaErrorInvalidValue;
    }
    return cudaGetLastError();
}

PLUGIN_EXPORT cudaError_t run_xor_delta_backward(
    const uint8_t* d_input,
    uint8_t* d_output,
    const size_t length_bytes,
    size_t type_size,
    size_t row_stride,
    cudaStream_t stream)
{
    (void)row_stride;
    if (length_bytes == 0 || type_size == 0) return cudaErrorInvalidValue;

    size_t num_elements = length_bytes / type_size;

    switch (type_size)
    {
    case 1: launch_xor_backward<uint8_t>(d_input, d_output, num_elements, stream); break;
    case 2: launch_xor_backward<uint16_t>(d_input, d_output, num_elements, stream); break;
    case 4: launch_xor_backward<uint32_t>(d_input, d_output, num_elements, stream); break;
    case 8: launch_xor_backward<uint64_t>(d_input, d_output, num_elements, stream); break;
    default: return cudaErrorInvalidValue;
    }
    return cudaGetLastError();
}
}
