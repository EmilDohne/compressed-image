#include <cuda_runtime.h>
#include <cub/cub.cuh>
#include <stdint.h>

/// \brief bytedelta forward kernel
///
/// This kernel applies a bytedelta operation to the `input`, modifying the `output` in-place.
///
/// \param input The input buffer that must have size `length`
/// \param output The output buffer that must have size `length`
/// \param length The byte length of the input and output buffers
/// \param stream_len The number of bytes per stream element
__global__ void bytedelta_forward_kernel(const uint8_t* input, uint8_t* output, size_t length, size_t stream_len)
{
    size_t tid = blockIdx.x * blockDim.x + threadIdx.x;

    if (tid < length)
    {
        int ip = tid % stream_len;
        if (ip == 0)
        {
            output[tid] = input[tid];
        }
        else
        {
            output[tid] = input[tid] - input[tid - 1];
        }
    }
}

/// \brief bytedelta backward kernel (Inverse operation)
///
/// This kernel reverses the bytedelta operation using a segmented inclusive
/// prefix-sum. Each block processes a single "type" (or channel) over its `stream_len`.
///
/// \tparam BLOCK_THREADS The number of threads per block. Must match the launch configuration.
/// \param input The input buffer containing the delta-encoded bytes
/// \param output The output buffer to store the decoded bytes
/// \param stream_len The number of bytes per segment/channel
/// \param typesize The total number of independent segments/channels
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
/// \brief C-API wrapper to launch the forward bytedelta kernel
///
/// Calculates grid dimensions based on the total `length` and launches the
/// forward kernel on the specified stream.
///
/// \param d_input Device pointer to the input buffer
/// \param d_output Device pointer to the output buffer
/// \param length Total size of the buffers in bytes
/// \param typesize The number of segments/channels
/// \param stream The CUDA stream to execute the kernel on
/// \return cudaError_t Returns cudaSuccess on success, or an error code upon failure
PLUGIN_EXPORT cudaError_t run_bytedelta_forward(
    const uint8_t* d_input,
    uint8_t* d_output,
    size_t length,
    size_t typesize,
    cudaStream_t stream)
{
    if (length <= 0 || typesize <= 0) return cudaErrorInvalidValue;

    int stream_len = length / typesize;
    int threads = 256;
    int blocks = (length + threads - 1) / threads;

    bytedelta_forward_kernel<<<blocks, threads, 0, stream>>>(d_input, d_output, length, stream_len);
    return cudaGetLastError();
}

/// \brief C-API wrapper to launch the backward bytedelta kernel
///
/// Configures the grid such that each block handles one `type_size` and launches
/// the backward kernel on the specified stream.
///
/// \param d_input Device pointer to the input buffer
/// \param d_output Device pointer to the output buffer
/// \param length Total size of the buffers in bytes
/// \param type_size The number of segments/channels
/// \param stream The CUDA stream to execute the kernel on
/// \return cudaError_t Returns cudaSuccess on success, or an error code upon failure
PLUGIN_EXPORT cudaError_t run_bytedelta_backward(
    const uint8_t* d_input,
    uint8_t* d_output,
    const size_t length,
    const size_t type_size,
    cudaStream_t stream)
{
    if (length <= 0 || type_size <= 0)
    {
        return cudaErrorInvalidValue;
    }

    const int stream_len = length / type_size;
    const int block_threads = 256;
    int blocks = type_size;

    bytedelta_backward_kernel<block_threads><<<blocks, block_threads, 0, stream>>>(
        d_input,
        d_output,
        stream_len,
        type_size
    );
    return cudaGetLastError();
}
}
