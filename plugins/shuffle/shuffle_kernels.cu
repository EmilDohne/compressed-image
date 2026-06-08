#include <cuda_runtime.h>
#include <stdint.h>

/// \brief Shuffle forward kernel (Byte shuffle)
///
/// This kernel applies a byte-shuffle operation to the `input`, separating 
/// contiguous elements by their byte index within the type.
///
/// \param input The input buffer that must have size `length`
/// \param output The output buffer that must have size `length`
/// \param length The total byte length of the input and output buffers
/// \param typesize The number of bytes per element (e.g., sizeof(float) = 4)
/// \param stream_len The number of elements in the buffer (length / typesize)
__global__ void shuffle_forward_kernel(const uint8_t* input,
                                       uint8_t* output,
                                       size_t length,
                                       size_t typesize,
                                       size_t stream_len)
{
    size_t tid = blockIdx.x * blockDim.x + threadIdx.x;

    if (tid < length)
    {
        // Treat input linearly. 
        // j is the element index, i is the byte offset within the element.
        size_t j = tid / typesize;
        size_t i = tid % typesize;

        // Write to transposed position: group all i-th bytes together
        output[i * stream_len + j] = input[tid];
    }
}

/// \brief Shuffle backward kernel (Byte unshuffle)
///
/// This kernel reverses the byte-shuffle operation, interleaving the grouped
/// bytes back into contiguous elements.
///
/// \param input The input buffer containing the shuffled bytes
/// \param output The output buffer to store the unshuffled bytes
/// \param length The total byte length of the input and output buffers
/// \param typesize The number of bytes per element
/// \param stream_len The number of elements in the buffer (length / typesize)
__global__ void shuffle_backward_kernel(const uint8_t* input,
                                        uint8_t* output,
                                        size_t length,
                                        size_t typesize,
                                        size_t stream_len)
{
    size_t tid = blockIdx.x * blockDim.x + threadIdx.x;

    if (tid < length)
    {
        // Treat input linearly (which is currently grouped by byte index).
        // i is the byte offset within the element, j is the element index.
        size_t i = tid / stream_len;
        size_t j = tid % stream_len;

        // Write back to interleaved position
        output[j * typesize + i] = input[tid];
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
/// \brief C-API wrapper to launch the forward shuffle kernel
///
/// Calculates grid dimensions based on the total `length` and launches the
/// forward kernel on the specified stream.
///
/// \param d_input Device pointer to the input buffer
/// \param d_output Device pointer to the output buffer
/// \param length Total size of the buffers in bytes
/// \param typesize The number of bytes per element
/// \param stream The CUDA stream to execute the kernel on
/// \return cudaError_t Returns cudaSuccess on success, or an error code upon failure
PLUGIN_EXPORT cudaError_t run_shuffle_forward(
    const uint8_t* d_input,
    uint8_t* d_output,
    size_t length,
    size_t typesize,
    cudaStream_t stream)
{
    if (length == 0 || typesize == 0) return cudaErrorInvalidValue;

    size_t stream_len = length / typesize;
    int threads = 256;
    size_t blocks = (length + threads - 1) / threads;

    shuffle_forward_kernel<<<blocks, threads, 0, stream>>>(
        d_input,
        d_output,
        length,
        typesize,
        stream_len
    );

    return cudaGetLastError();
}

/// \brief C-API wrapper to launch the backward shuffle (unshuffle) kernel
///
/// Calculates grid dimensions based on the total `length` and launches the
/// backward kernel on the specified stream.
///
/// \param d_input Device pointer to the input buffer (shuffled data)
/// \param d_output Device pointer to the output buffer (unshuffled data)
/// \param length Total size of the buffers in bytes
/// \param typesize The number of bytes per element
/// \param stream The CUDA stream to execute the kernel on
/// \return cudaError_t Returns cudaSuccess on success, or an error code upon failure
PLUGIN_EXPORT cudaError_t run_shuffle_backward(
    const uint8_t* d_input,
    uint8_t* d_output,
    const size_t length,
    const size_t typesize,
    cudaStream_t stream)
{
    if (length == 0 || typesize == 0) return cudaErrorInvalidValue;

    size_t stream_len = length / typesize;
    int threads = 256;
    int blocks = (length + threads - 1) / threads;

    shuffle_backward_kernel<<<blocks, threads, 0, stream>>>(
        d_input,
        d_output,
        length,
        typesize,
        stream_len
    );

    return cudaGetLastError();
}
}
