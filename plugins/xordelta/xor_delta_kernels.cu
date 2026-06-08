#include <cuda_runtime.h>
#include <cub/cub.cuh>
#include <stdint.h>

// ##########################################################################
// Functors
// ##########################################################################

/// \brief Functor for computing the bitwise XOR of two elements in CUB scans
struct XorOp
{
    template <typename T>
    __device__ __forceinline__ T operator()(const T& a, const T& b) const
    {
        return a ^ b;
    }
};

// ##########################################################################
// XOR Delta Kernels (Bitwise XOR)
// ##########################################################################

template <typename T>
__global__ void xor_delta_forward_kernel(const T* input, T* output, size_t num_elements, size_t stream_len)
{
    size_t tid = blockIdx.x * blockDim.x + threadIdx.x;

    if (tid < num_elements)
    {
        size_t ip = tid % stream_len;
        if (ip == 0)
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
__global__ void xor_delta_backward_kernel(const T* input, T* output, size_t stream_len, size_t num_streams)
{
    size_t ich = blockIdx.x;
    if (ich >= num_streams) return;

    size_t offset = ich * stream_len;

    typedef cub::BlockScan<T, BLOCK_THREADS> BlockScan;
    __shared__ typename BlockScan::TempStorage temp_storage;

    T carry = 0;

    for (size_t i_base = 0; i_base < stream_len; i_base += BLOCK_THREADS)
    {
        size_t i = i_base + threadIdx.x;
        bool valid = (i < stream_len);

        T thread_data = valid ? input[offset + i] : 0;
        T block_sum;

        // XOR is its own inverse; we use a custom XorOp for the prefix scan
        BlockScan(temp_storage).InclusiveScan(thread_data, thread_data, XorOp(), block_sum);

        if (valid)
        {
            output[offset + i] = thread_data ^ carry;
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

extern "C" {
PLUGIN_EXPORT cudaError_t run_xor_delta_forward(
    const uint8_t* d_input,
    uint8_t* d_output,
    size_t length_bytes,
    size_t type_size,
    cudaStream_t stream)
{
    if (length_bytes == 0 || type_size == 0) return cudaErrorInvalidValue;

    size_t num_elements = length_bytes / type_size;

    // For a flat 1D delta, the stream length is the entire array
    size_t stream_len = num_elements;

    int threads = 256;
    int blocks = (num_elements + threads - 1) / threads;

    switch (type_size)
    {
    case 1:
        {
            // Already uint8_t, but scoped for consistency
            const auto* in_ptr = d_input;
            auto* out_ptr = d_output;
            xor_delta_forward_kernel<uint8_t><<<blocks, threads, 0, stream>>>(
                in_ptr,
                out_ptr,
                num_elements,
                stream_len
            );
            break;
        }
    case 2:
        {
            const auto* in_ptr = reinterpret_cast<const uint16_t*>(d_input);
            auto* out_ptr = reinterpret_cast<uint16_t*>(d_output);
            xor_delta_forward_kernel<uint16_t><<<blocks, threads, 0, stream>>>(
                in_ptr,
                out_ptr,
                num_elements,
                stream_len
            );
            break;
        }
    case 4:
        {
            const auto* in_ptr = reinterpret_cast<const uint32_t*>(d_input);
            auto* out_ptr = reinterpret_cast<uint32_t*>(d_output);
            xor_delta_forward_kernel<uint32_t><<<blocks, threads, 0, stream>>>(
                in_ptr,
                out_ptr,
                num_elements,
                stream_len
            );
            break;
        }
    case 8:
        {
            const auto* in_ptr = reinterpret_cast<const uint64_t*>(d_input);
            auto* out_ptr = reinterpret_cast<uint64_t*>(d_output);
            xor_delta_forward_kernel<uint64_t><<<blocks, threads, 0, stream>>>(
                in_ptr,
                out_ptr,
                num_elements,
                stream_len
            );
            break;
        }
    default: return cudaErrorInvalidValue;
    }
    return cudaGetLastError();
}

PLUGIN_EXPORT cudaError_t run_xor_delta_backward(
    const uint8_t* d_input,
    uint8_t* d_output,
    const size_t length_bytes,
    size_t type_size,
    cudaStream_t stream)
{
    if (length_bytes == 0 || type_size == 0) return cudaErrorInvalidValue;

    size_t num_elements = length_bytes / type_size;

    // For a flat 1D delta, there is 1 stream that is the length of the array
    size_t stream_len = num_elements;
    size_t num_streams = 1;

    const int block_threads = 256;
    int blocks = num_streams;

    switch (type_size)
    {
    case 1:
        {
            const auto* in_ptr = d_input;
            auto* out_ptr = d_output;
            xor_delta_backward_kernel<uint8_t, block_threads><<<blocks, block_threads, 0, stream>>>(
                in_ptr,
                out_ptr,
                stream_len,
                num_streams
            );
            break;
        }
    case 2:
        {
            const auto* in_ptr = reinterpret_cast<const uint16_t*>(d_input);
            auto* out_ptr = reinterpret_cast<uint16_t*>(d_output);
            xor_delta_backward_kernel<uint16_t, block_threads><<<blocks, block_threads, 0, stream>>>(
                in_ptr,
                out_ptr,
                stream_len,
                num_streams
            );
            break;
        }
    case 4:
        {
            const auto* in_ptr = reinterpret_cast<const uint32_t*>(d_input);
            auto* out_ptr = reinterpret_cast<uint32_t*>(d_output);
            xor_delta_backward_kernel<uint32_t, block_threads><<<blocks, block_threads, 0, stream>>>(
                in_ptr,
                out_ptr,
                stream_len,
                num_streams
            );
            break;
        }
    case 8:
        {
            const auto* in_ptr = reinterpret_cast<const uint64_t*>(d_input);
            auto* out_ptr = reinterpret_cast<uint64_t*>(d_output);
            xor_delta_backward_kernel<uint64_t, block_threads><<<blocks, block_threads, 0, stream>>>(
                in_ptr,
                out_ptr,
                stream_len,
                num_streams
            );
            break;
        }
    default: return cudaErrorInvalidValue;
    }
    return cudaGetLastError();
}
}
