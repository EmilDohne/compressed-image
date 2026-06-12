#include <cuda_runtime.h>
#include <cub/cub.cuh>
#include <stdint.h>

// ##########################################################################
// bytedelta kernels
// ##########################################################################
//
// Runs on already-shuffled data: the buffer is `typesize` independent byte lanes, each
// `stream_len = length / typesize` bytes long. Flat 1D horizontal byte delta within each lane.
// `row_stride` is accepted only for ABI parity and is ignored.

__global__ void bytedelta_forward_kernel(const uint8_t* input, uint8_t* output, size_t length, size_t stream_len)
{
    size_t tid = blockIdx.x * blockDim.x + threadIdx.x;

    if (tid < length)
    {
        if (tid % stream_len == 0)
        {
            output[tid] = input[tid];
        }
        else
        {
            output[tid] = input[tid] - input[tid - 1];
        }
    }
}

template <int BLOCK_THREADS>
__global__ void bytedelta_backward_kernel(const uint8_t* input, uint8_t* output, size_t stream_len, size_t typesize)
{
    size_t ich = blockIdx.x;
    if (ich >= typesize) return;

    size_t offset = ich * stream_len;

    typedef cub::BlockScan<uint8_t, BLOCK_THREADS> BlockScan;
    __shared__ typename BlockScan::TempStorage temp_storage;

    uint8_t carry = 0;

    for (size_t i_base = 0; i_base < stream_len; i_base += BLOCK_THREADS)
    {
        size_t i = i_base + threadIdx.x;
        bool valid = (i < stream_len);

        uint8_t thread_data = valid ? input[offset + i] : 0;
        uint8_t block_sum;

        BlockScan(temp_storage).InclusiveSum(thread_data, thread_data, block_sum);

        if (valid)
        {
            output[offset + i] = thread_data + carry;
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

extern "C" {
PLUGIN_EXPORT cudaError_t run_bytedelta_forward(
    const uint8_t* d_input,
    uint8_t* d_output,
    size_t length,
    size_t typesize,
    size_t row_stride,
    cudaStream_t stream)
{
    (void)row_stride; // accepted for ABI parity; this 1D filter does not use it.
    if (length == 0 || typesize == 0) return cudaErrorInvalidValue;

    size_t stream_len = length / typesize;
    int threads = 256;
    size_t blocks = (length + threads - 1) / threads;

    bytedelta_forward_kernel<<<blocks, threads, 0, stream>>>(d_input, d_output, length, stream_len);
    return cudaGetLastError();
}

PLUGIN_EXPORT cudaError_t run_bytedelta_backward(
    const uint8_t* d_input,
    uint8_t* d_output,
    const size_t length,
    const size_t type_size,
    size_t row_stride,
    cudaStream_t stream)
{
    (void)row_stride;
    if (length == 0 || type_size == 0) return cudaErrorInvalidValue;

    const size_t stream_len = length / type_size;
    const int block_threads = 256;

    bytedelta_backward_kernel<block_threads><<<type_size, block_threads, 0, stream>>>(
        d_input,
        d_output,
        stream_len,
        type_size
    );
    return cudaGetLastError();
}
}
