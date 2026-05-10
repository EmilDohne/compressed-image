/*
Entry point for the various compressors of nvcomp
*/

#pragma once

#include <variant>
#include <cstddef>
#include <vector>
#include <ranges>
#include <execution>

#include <nvcomp.h>
#include <nvcomp/lz4.h>
#include <nvcomp/cascaded.h>
#include <nvcomp/deflate.h>
#include <nvcomp/gdeflate.h>
#include <nvcomp/gzip.h>
#include <nvcomp/snappy.h>
#include <nvcomp/zstd.h>

#include "compressed/macros.h"
#include "compressed/constants.h"
#include "compressed/enums.h"
#include "compressed/util.h"

#include "compressed/cuda/memory.h"
#include "compressed/cuda/enums.h"
#include "compressed/cuda/compressors/util.h"
#include "compressed/cuda/cuda_hook.h"
#include "compressed/cuda/gpu.h"


namespace
NAMESPACE_COMPRESSED_IMAGE
{
    namespace cuda
    {
        /// \brief compression options for the various nvcomp compressors.
        using compression_options = std::variant<
            nvcompBatchedLZ4CompressOpts_t,
            nvcompBatchedCascadedCompressOpts_t,
            nvcompBatchedDeflateCompressOpts_t,
            nvcompBatchedGdeflateCompressOpts_t,
            nvcompBatchedGzipCompressOpts_t,
            nvcompBatchedSnappyCompressOpts_t,
            nvcompBatchedZstdCompressOpts_t
        >;

        /// \brief decompression options for the various nvcomp compressors.
        using decompression_options = std::variant<
            nvcompBatchedLZ4DecompressOpts_t,
            nvcompBatchedCascadedDecompressOpts_t,
            nvcompBatchedDeflateDecompressOpts_t,
            nvcompBatchedGdeflateDecompressOpts_t,
            nvcompBatchedGzipDecompressOpts_t,
            nvcompBatchedSnappyDecompressOpts_t,
            nvcompBatchedZstdDecompressOpts_t
        >;


        /// \brief Compression context for nvcomp/cuda based compression and decompression.
        ///
        /// Similar in scope to a `blosc2_context` struct but instead holds gpu-specific information.
        struct nvcomp_context
        {
            /// The options to use for compression
            compression_options comp_options{};

            /// The options to use for decompression
            decompression_options decomp_options{};

            /// The block size used for compression. All blocks will have this size except for the last one which may
            /// be smaller. The input is split into this many blocks. If the requested block size exceeds what the
            /// compressor allows, it is internally reduced. A typical recommended value is 65536 (2^16).
            size_t block_size = s_default_blocksize;

            /// The compression codec to be used. Must be one of the GPU codecs to be valid.
            NAMESPACE_COMPRESSED_IMAGE::enums::codec codec{};

            /// The GPU device to use for compression/decompression.
            int gpu_device = 0;
        };


        /// A single compressed chunk holding a collection of blocks inside it. Similar to a blosc2 chunk but instead
        /// of being stored as a single chunk
        ///
        /// .. note::
        ///		The block sizes of the chunk will always be equal to `max_block_size` except for the last block which
        ///		may be smaller.
        template <typename T>
        struct compressed_chunk
        {
            /// \brief The compressed data, stored on host as a std::vector.
            std::vector<NAMESPACE_COMPRESSED_IMAGE::util::default_init_vector<std::byte>> blocks;

            /// \brief The uncompressed block sizes of all the blocks inside `blocks`. Expressed as bytes
            std::vector<size_t> block_sizes{};

            /// \brief The compression context used for compression/decompression. Once set this may not be modified.
            nvcomp_context context{};

            [[nodiscard]] size_t csize() const
            {
                return std::accumulate(this->blocks.begin(), this->blocks.end(), 0,
                                       [](size_t sum, const auto& elem)
                                       {
                                           return sum + elem.size();
                                       });
            }

            [[nodiscard]] size_t size() const
            {
                return this->byte_size() / sizeof(T);
            }

            [[nodiscard]] size_t byte_size() const
            {
                return std::accumulate(block_sizes.begin(), block_sizes.end(), 0);
            }

            [[nodiscard]] inline size_t max_block_size() const
            {
                if (block_sizes.empty())
                    return 0;

                return block_sizes.at(0);
            }
        };


        namespace detail
        {
            /// \brief base cuda-based compressor base that provides utility functions for the various compressor implementations
            ///
            /// Note: we use blosc2 terminology for a lot of these calls i.e. what nvcomp may call 'chunks' we call blocks
            ///		  as each unit we are compressing is already a chunk. This gives us the same 3D data structure where
            ///		  we cascade from channel -> chunks -> blocks. Unlike with blosc2 the blocks here are transparent to us
            ///		  but we hide them from the call site
            ///
            /// When re-implementing this for different compression procedures, one needs to implement:
            ///
            /// - codec							<-- The codec associated with this compressor
            /// - default_compression_opts		<-- Default compression options
            /// - default_decompression_opts	<-- Default decompression options
            /// - get_temp_bytes				<-- The number of scratch bytes required for compression/decompression
            /// - block_max_compressed_size		<-- The max block size of a compressed block, given the compression opts
            /// - max_block_size				<-- The overall max (uncompressed) size a block may have.
            /// - compression_impl				<-- The implementation of the compression procedure
            /// - decompression_impl			<-- The implementation of the decompression procedure
            template <typename T>
            struct compressor
            {
                virtual ~compressor() = default;

                /// \brief Compress a CPU buffer into a compressed chunk using CUDA.
                ///
                /// This function transfers the input data to the specified CUDA device, splits it
                /// into fixed-size blocks, compresses each block asynchronously, and copies the
                /// compressed results back to host memory. The compression is performed using
                /// the algorithm configured by \p context.
                ///
                /// \param data        The input data buffer to compress (host memory).
                /// \param context	   The compression/decompression context used for the generated blocks.
                ///
                /// \return A \c compressed_chunk containing the compressed blocks, their sizes,
                ///         and codec metadata describing the compression algorithm used.
                ///
                /// \throws std::bad_variant_access  If the provided \p options are not valid for
                ///                                  the current compression algorithm.
                /// \throws std::runtime_error       If any CUDA memory allocation, copy, or kernel
                ///                                  execution fails.
                ///
                /// \note Compression uses asynchronous CUDA operations with per-thread streams and
                ///       memory pooling. Data is synchronized before returning, but overlapping
                ///       work on other streams may proceed concurrently.
                compressed_chunk<T> compress(std::span<T> data, nvcomp_context context) const
                {
                    // Set both the current stack device as well as unlocking the mem pool size allowing
                    // future calls to `compress` to take advantage of this memory pooling.
                    device_guard guard(context.gpu_device);
                    cuda_api::instance().set_mem_pool_size(context.gpu_device);

                    // Ensure the block size does not exceed the max the compressor allows for.
                    context.block_size = this->fit_block_size(context.block_size);

                    // ##################################################################################
                    // Set up device uncompressed memory
                    // ##################################################################################

                    // Compute the total number of blocks as well as a vector of all the block sizes
                    const size_t num_blocks = (data.size() * sizeof(T) + context.block_size - 1) / context.block_size;
                    auto block_sizes = this->generate_block_sizes(data.size() * sizeof(T), context.block_size,
                                                                  num_blocks);

                    // Allocate the buffer for `data` on the device and memcpy over. Note that we do this using the
                    // asynchronous API giving us two benefits:
                    // - Other threads (streams) may do other operations in-between
                    // - It uses the cudaMallocAsync/cudaMallocFree under the hood which is memory-pooled speeding up
                    //   the allocations tremendously as we can re-use other previously freed memory!
                    auto device_uncompressed_data = make_device_buffer_async<T>(data.size());
                    cuda_api::instance().memcpy_async(
                        device_uncompressed_data.get_raw(),
                        static_cast<void*>(data.data()),
                        device_uncompressed_data.bytes(),
                        cudaMemcpyHostToDevice
                    );

                    // Generate the device memory for the uncompressed block pointers and block sizes
                    auto device_block_pointers = compressor::generate_device_block_pointers(
                        device_uncompressed_data,
                        context.block_size,
                        num_blocks
                    );
                    auto device_block_sizes = cuda_device_buffer_async<size_t>::from_host(block_sizes);

                    // ##################################################################################
                    // Set up device compressed memory
                    // ##################################################################################

                    auto max_block_compressed_size = this->block_max_compressed_size(
                        context.block_size, context.comp_options);

                    // Now, allocate the memory on the device for the output buffers
                    std::vector<void*> host_compressed_ptrs(num_blocks);
                    std::vector<cuda_device_buffer_async<std::byte>> _device_buffers(num_blocks);
                    for (size_t i = 0; i < num_blocks; ++i)
                    {
                        _device_buffers[i] = make_device_buffer_async<std::byte>(max_block_compressed_size);
                        host_compressed_ptrs[i] = _device_buffers[i].get_raw();
                    }
                    // Make sure the pointers live on the gpu as well.
                    auto device_compressed_ptrs = cuda_device_buffer_async<void*>::from_host(host_compressed_ptrs);

                    // ##################################################################################
                    // Set up scratch buffer
                    // ##################################################################################

                    auto device_temp_compression_buffer = this->generate_temp_buffer(
                        context.block_size,
                        num_blocks,
                        context.comp_options
                    );

                    // ##################################################################################
                    // Call the compression routine (implementation-defined)
                    // ##################################################################################

                    // The compressed byte sizes
                    auto device_compressed_bytes = make_device_buffer_async<size_t>(num_blocks);
                    auto device_statuses = make_device_buffer_async<nvcompStatus_t>(num_blocks);

                    this->compression_impl(
                        context.block_size,
                        num_blocks,
                        device_block_pointers,
                        device_block_sizes,
                        device_temp_compression_buffer,
                        device_compressed_ptrs,
                        device_compressed_bytes,
                        device_statuses,
                        context.comp_options
                    );

                    // ##################################################################################
                    // Copy the compressed data back to host
                    // ##################################################################################

                    std::vector<NAMESPACE_COMPRESSED_IMAGE::util::default_init_vector<std::byte>> compressed_blocks(
                        num_blocks);
                    std::vector<size_t> compressed_bytes(num_blocks);
                    device_compressed_bytes.
                        to_host(std::span<size_t>(compressed_bytes.begin(), compressed_bytes.end()));

                    auto gen = std::views::iota(size_t{0}, num_blocks);
                    std::for_each(std::execution::seq, gen.begin(), gen.end(), [&](size_t block_idx)
                    {
                        // Allocate memory
                        auto& block = compressed_blocks.at(block_idx);
                        block = NAMESPACE_COMPRESSED_IMAGE::util::default_init_vector<std::byte>(
                            compressed_bytes.at(block_idx));

                        // Copy from device back to host
                        cuda_api::instance().memcpy_async(
                            (block.data()),
                            host_compressed_ptrs.at(block_idx),
                            block.size(),
                            cudaMemcpyDeviceToHost
                        );
                    });

                    // ##################################################################################
                    // Synchronize, check for errors and return
                    // ##################################################################################

                    cuda_api::instance().stream_synchronize(cudaStreamPerThread);

                    compressor<T>::validate_per_block_statuses(device_statuses);

                    return compressed_chunk<T>{
                        std::move(compressed_blocks),
                        std::move(block_sizes),
                        std::move(context)
                    };
                };


                /// \brief Decompress a compressed_chunk directly into a preallocated CPU buffer.
                ///
                /// This function decompresses all blocks in \p chunk and writes the output
                /// sequentially into \p output. The caller must ensure that \p output is
                /// large enough to hold the full decompressed data.
                ///
                /// \param chunk     The compressed data (blocks + sizes).
                /// \param output    The preallocated span of memory where the uncompressed data
                ///                  will be written.
                ///
                /// \throws std::runtime_error on CUDA or nvCOMP failure.
                void decompress(const compressed_chunk<T>& chunk, std::span<T> output) const
                {
                    device_guard guard(chunk.context.gpu_device);
                    cuda_api::instance().set_mem_pool_size(chunk.context.gpu_device);

                    const size_t num_blocks = chunk.blocks.size();

                    // ##################################################################################
                    // Upload compressed blocks to device
                    // ##################################################################################
                    std::vector<cuda_device_buffer_async<std::byte>> device_compressed_blocks(num_blocks);
                    std::vector<void*> host_compressed_ptrs(num_blocks);
                    std::vector<size_t> host_compressed_bytes(num_blocks);

                    for (size_t i = 0; i < num_blocks; ++i)
                    {
                        device_compressed_blocks[i] = cuda_device_buffer_async<std::byte>::from_host(
                            chunk.blocks[i].data(),
                            chunk.blocks[i].size()
                        );

                        host_compressed_ptrs[i] = device_compressed_blocks[i].get_raw();
                        host_compressed_bytes[i] = chunk.blocks[i].size();
                    }

                    auto device_compressed_ptrs = cuda_device_buffer_async<void*>::from_host(host_compressed_ptrs);
                    auto device_compressed_bytes = cuda_device_buffer_async<size_t>::from_host(host_compressed_bytes);

                    // ##################################################################################
                    // Allocate single contiguous device buffer for all output
                    // ##################################################################################
                    auto device_output = make_device_buffer_async<T>(output.size());

                    // Generate per-block device pointers (offsets into single buffer)
                    std::vector<void*> host_uncompressed_ptrs(num_blocks);
                    size_t offset_bytes = 0;
                    for (size_t i = 0; i < num_blocks; ++i)
                    {
                        host_uncompressed_ptrs[i] = reinterpret_cast<void*>(device_output.get() + (offset_bytes / sizeof
                            (T)));
                        offset_bytes += chunk.context.block_size;
                    }

                    auto device_uncompressed_ptrs = cuda_device_buffer_async<void*>::from_host(host_uncompressed_ptrs);
                    auto device_uncompressed_bytes = cuda_device_buffer_async<size_t>::from_host(chunk.block_sizes);

                    // ##################################################################################
                    // Allocate scratch buffer for decompression
                    // ##################################################################################

                    auto device_temp = this->generate_temp_buffer(chunk.context.block_size, num_blocks,
                                                                  chunk.context.decomp_options);
                    auto device_statuses = make_device_buffer_async<nvcompStatus_t>(num_blocks);

                    // ##################################################################################
                    // Call algorithm-specific device decompression
                    // ##################################################################################
                    decompression_impl(
                        chunk.context.block_size,
                        num_blocks,
                        device_compressed_ptrs,
                        device_compressed_bytes,
                        device_temp,
                        device_uncompressed_ptrs,
                        device_uncompressed_bytes,
                        device_statuses,
                        chunk.context.decomp_options
                    );

                    // ##################################################################################
                    // Copy result back to host
                    // ##################################################################################
                    cuda_api::instance().memcpy_async(
                        static_cast<void*>(output.data()),
                        device_output.get_raw(),
                        output.size() * sizeof(T),
                        cudaMemcpyDeviceToHost);

                    cuda_api::instance().stream_synchronize(cudaStreamPerThread);

                    compressor<T>::validate_per_block_statuses(device_statuses);
                }

                /// \brief The codec associated with the compressor.
                [[nodiscard]] virtual NAMESPACE_COMPRESSED_IMAGE::enums::codec codec() const noexcept = 0;

                [[nodiscard]] virtual compression_options default_compression_opts() const noexcept = 0;
                [[nodiscard]] virtual decompression_options default_decompression_opts() const noexcept = 0;

                /// \brief The max block size allowed for a given compressor. Implementation defined.
                [[nodiscard]] virtual size_t max_block_size() const noexcept = 0;

                /// \brief Fits the given `block_size` to be <= what the compressor allows.
                ///
                /// Additionally, ensures the block size aligns to T such that we can go back and forth from
                /// std::byte <-> T cleanly.
                [[nodiscard]] size_t fit_block_size(size_t block_size) const noexcept
                {
                    auto fitted = std::min(block_size, this->max_block_size());
                    fitted -= fitted % sizeof(T);
                    return fitted;
                };

             private:
                /// ##################################################################################
                /// Pure virtual function, dependent on compressor.
                /// ##################################################################################

                /// \brief Retrieve the number of temporary device bytes needed for the compression/decompression procedure
                ///
                /// \param block_size The block size of one of the sub-streams
                /// \param num_blocks The total number of blocks
                /// \param options	  The compression/decompression options for which to get the number of temporary
                ///					  bytes
                virtual size_t get_temp_bytes(
                    size_t block_size,
                    size_t num_blocks,
                    std::variant<compression_options, decompression_options> options
                ) = 0;

                /// \brief Retrieve the maximum size required for compressing a single block for the given options
                ///
                /// \param block_size The size of a single block
                /// \param options	  The compression options which are used for compression.
                virtual size_t block_max_compressed_size(size_t block_size, compression_options& options) = 0;


                /// \brief Call the underlying compression implementation of the set of blocks.
                ///
                /// All memory allocation needs to happen before this point, as this assumes all of these buffers have
                /// been allocated and filled
                ///
                /// \param block_size				The overall block size, all blocks except for the last should have
                ///									this size.
                /// \param num_blocks				The overall number of blocks
                /// \param uncompressed_block_ptrs	The pointers to the start of each uncompressed block
                /// \param uncompressed_block_sizes	The size of each uncompressed block
                /// \param scratch_space			The scratch bytes used by the compressor during compression.
                /// \param compressed_block_ptrs	To be filled out by the implementation, the pointers to the start
                ///									of each (preallocated) compressed block
                /// \param compressed_block_sizes	To be filled out by the implementation, the sizes of the compressed
                ///									blocks.
                /// \param block_statuses			The statuses per compressed block, these live on the GPU and must be
                ///									copied back for introspection.
                /// \param options					The compression options to use, must be valid for the current
                ///									compressor.
                virtual void compression_impl(
                    size_t block_size,
                    size_t num_blocks,
                    const cuda_device_buffer_async<void*>& uncompressed_block_ptrs,
                    const cuda_device_buffer_async<size_t>& uncompressed_block_sizes,
                    cuda_device_buffer_async<std::byte>& scratch_space,
                    cuda_device_buffer_async<void*>& compressed_block_ptrs,
                    cuda_device_buffer_async<size_t>& compressed_block_sizes,
                    cuda_device_buffer_async<nvcompStatus_t>& block_statuses,
                    const compression_options& options
                ) const = 0;

                /// \brief Low-level device decompression implementation.
                ///
                /// This function should be implemented by the derived class for a specific
                /// compression algorithm (e.g., lz, zstd). It operates entirely on device
                /// memory and writes decompressed data into `uncompressed_block_ptrs`.
                ///
                /// \param block_size               The overall block size, all blocks except for the last should have
                ///									this size.
                /// \param num_blocks               The overall number of blocks
                /// \param compressed_block_ptrs    Device buffer containing pointers to compressed blocks.
                /// \param compressed_block_sizes   Device buffer containing sizes of compressed blocks.
                /// \param scratch_space            Temporary device buffer allocated for decompression.
                /// \param uncompressed_block_ptrs	The pointers to the start of each uncompressed block, filled out by
                ///									this function
                /// \param uncompressed_block_sizes	The size of each uncompressed block, filled out by this function.
                /// \param block_statuses			The statuses per compressed block, these live on the GPU and must be
                ///									copied back for introspection.
                /// \param options                  Algorithm-specific decompression options.
                virtual void decompression_impl(
                    size_t block_size,
                    size_t num_blocks,
                    const cuda_device_buffer_async<void*>& compressed_block_ptrs,
                    const cuda_device_buffer_async<size_t>& compressed_block_sizes,
                    cuda_device_buffer_async<std::byte>& scratch_space,
                    cuda_device_buffer_async<void*>& uncompressed_block_ptrs,
                    cuda_device_buffer_async<size_t>& uncompressed_block_sizes,
                    cuda_device_buffer_async<nvcompStatus_t>& block_statuses,
                    const decompression_options& options
                ) const = 0;

            private:
                /// ##################################################################################
                /// Generic functions across all compressors
                /// ##################################################################################


                /// \brief Validate all the errors per-block collected during compression/decompression and throw these
                ///		   as aggregated exception
                ///
                /// This function NEEDS to be called after synchronization of the stream and the compression/decompression
                /// operations as otherwise these statuses are not yet guaranteed to be valid.
                ///
                /// \param device_statuses The device-memory held nvcompStatus_t vector.
                void validate_per_block_statuses(cuda_device_buffer_async<nvcompStatus_t>& device_statuses) const
                {
                    // Now that we have synchronized, we can check the statuses per-block
                    auto status_vector = NAMESPACE_COMPRESSED_IMAGE::util::default_init_vector<nvcompStatus_t>(
                        device_statuses.size);

                    // Copy from device back to host
                    cuda_api::instance().memcpy_async(
                        status_vector.data(),
                        device_statuses.get(),
                        device_statuses.bytes(),
                        cudaMemcpyDeviceToHost
                    );

                    // Aggregate errors instead of throwing immediately
                    std::vector<std::string> error_messages;
                    for (size_t i = 0; i < device_statuses.size; ++i)
                    {
                        if (status_vector[i] != nvcompStatus_t::nvcompSuccess)
                        {
                            error_messages.emplace_back(
                                std::format(
                                    "block {} failed with nvcomp status: '{}'",
                                    i,
                                    cuda::util::status_t_to_string(status_vector[i])
                                )
                            );
                        }
                    }

                    // If any errors occurred, combine and throw a single aggregated exception
                    if (!error_messages.empty())
                    {
                        std::string joined_errors;
                        joined_errors.reserve(error_messages.size() * 64);

                        for (size_t j = 0; j < error_messages.size(); ++j)
                        {
                            joined_errors += error_messages[j];
                            if (j + 1 < error_messages.size())
                            {
                                joined_errors += '\n';
                            }
                        }

                        throw std::runtime_error(
                            std::format(
                                "compression/decompression failed for {} out of {} blocks:\n{}",
                                error_messages.size(),
                                device_statuses.size(),
                                joined_errors
                            )
                        );
                    }
                }


                /// \brief Generates the temporary buffer the compressor uses internally.
                ///
                /// \param block_size The block size to use for compression
                /// \param num_blocks The number of blocks to compress
                /// \param options The compression/decompression options that will be used
                cuda_device_buffer_async<std::byte> generate_temp_buffer(
                    const size_t block_size,
                    const size_t num_blocks,
                    std::variant<compression_options, decompression_options> options
                )
                {
                    auto size = this->get_temp_bytes(block_size, num_blocks, options);
                    return make_device_buffer_async<std::byte>(size);
                }

                /// \brief Generate a buffer (vector) containing pointers into the individual blocks
                ///
                /// These pointers index into the device memory from `device_buffer`
                ///
                /// \param device_uncompressed_data The device memory buffer which holds the uncompressed data
                /// \param block_size The size of a single block
                /// \param num_blocks The number of blocks `device_buffer` stores
                static cuda_device_buffer_async<void*> generate_device_block_pointers(
                    const cuda_device_buffer_async<T>& device_uncompressed_data,
                    const size_t block_size,
                    const size_t num_blocks
                )
                {
                    std::vector<void*> ptrs(num_blocks);

                    auto device_base_ptr = static_cast<char*>(device_uncompressed_data.get_raw());
                    for (size_t i = 0; i < num_blocks; ++i)
                    {
                        ptrs[i] = device_base_ptr + block_size * i;
                    }

                    // Now that we have this memory on the host, we memcpy it over
                    auto device_buffer = make_device_buffer_async<void*>(num_blocks);
                    cuda_api::instance().memcpy_async(
                        device_buffer.get_raw(),
                        ptrs.data(),
                        device_buffer.bytes(),
                        cudaMemcpyDeviceToDevice
                    );

                    return std::move(device_buffer);
                }

                /// \brief Compute a vector of all the block sizes of the uncompressed data.
                ///
                /// All elements will be == to `block_size` except the last element which will be the mod.
                ///
                /// \param num_bytes The total uncompressed bytes
                /// \param block_size The block size, already fitted to fit within
                /// \param num_blocks The number of blocks to generate.
                std::vector<size_t> generate_block_sizes(
                    const size_t num_bytes,
                    const size_t block_size,
                    const size_t num_blocks
                )
                {
                    std::vector<size_t> out(num_blocks, block_size);
                    if (!out.empty())
                    {
                        out[out.size() - 1] = num_bytes - (block_size * (num_blocks - 1));
                    }
                    return out;
                }
            };
        }
    } // namespace cuda
} // namespace NAMESPACE_COMPRESSED_IMAGE
