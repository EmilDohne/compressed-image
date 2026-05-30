#pragma once

#include "compressed/macros.h"

#include "compressed/cuda/compressors/base.h"
#include "compressed/cuda/compressors/util.h"

namespace
NAMESPACE_COMPRESSED_IMAGE
{
    namespace cuda
    {
        template <typename T>
        struct snappy_compressor final : public detail::compressor<T>
        {
            [[nodiscard]] NAMESPACE_COMPRESSED_IMAGE::enums::codec codec() const noexcept override
            {
                return NAMESPACE_COMPRESSED_IMAGE::enums::codec::snappy_gpu;
            };

            [[nodiscard]] compression_options default_compression_opts() const noexcept override
            {
                return nvcompBatchedSnappyCompressDefaultOpts;
            };

            [[nodiscard]] decompression_options default_decompression_opts() const noexcept override
            {
                return nvcompBatchedSnappyDecompressDefaultOpts;
            };

            [[nodiscard]] size_t max_block_size() const noexcept override
            {
                return nvcompSnappyCompressionMaxAllowedChunkSize;
            };

        private:
            size_t get_temp_bytes(
                size_t block_size,
                size_t num_blocks,
                std::variant<compression_options, decompression_options> options
            ) const override
            {
                if (std::holds_alternative<compression_options>(options))
                {
                    size_t temp_bytes{};
                    const auto status = nvcompBatchedSnappyCompressGetTempSizeAsync(
                        num_blocks,
                        block_size,
                        std::get<nvcompBatchedSnappyCompressOpts_t>(std::get<compression_options>(options)),
                        &temp_bytes,
                        block_size * num_blocks
                    );

                    if (status != nvcompStatus_t::nvcompSuccess)
                    {
                        throw std::runtime_error(
                            std::format(
                                "snappy: Unable to retrieve the scratch bytes required for compression due to nvcomp error: '{}'",
                                util::status_t_to_string(status)
                            )
                        );
                    }

                    return temp_bytes;
                }

                size_t temp_bytes{};
                const auto status = nvcompBatchedSnappyDecompressGetTempSizeAsync(
                    num_blocks,
                    block_size,
                    std::get<nvcompBatchedSnappyDecompressOpts_t>(std::get<decompression_options>(options)),
                    &temp_bytes,
                    block_size * num_blocks
                );

                if (status != nvcompStatus_t::nvcompSuccess)
                {
                    throw std::runtime_error(
                        std::format(
                            "snappy: Unable to retrieve the scratch bytes required for decompression due to nvcomp error: '{}'",
                            util::status_t_to_string(status)
                        )
                    );
                }

                return temp_bytes;
            }


            size_t block_max_compressed_size(size_t block_size, compression_options& options) const override
            {
                size_t max_bytes = 0;
                const auto status = nvcompBatchedSnappyCompressGetMaxOutputChunkSize(
                    block_size,
                    std::get<nvcompBatchedSnappyCompressOpts_t>(options),
                    &max_bytes
                );

                if (status != nvcompStatus_t::nvcompSuccess)
                {
                    throw std::runtime_error(
                        std::format(
                            "snappy: Unable to retrieve the maximum compressed size for a block due to nvcomp error: '{}'",
                            util::status_t_to_string(status)
                        )
                    );
                }

                return max_bytes;
            }


            void compression_impl(
                size_t block_size,
                size_t num_blocks,
                const cuda_device_buffer_async<void*>& uncompressed_block_ptrs,
                const cuda_device_buffer_async<size_t>& uncompressed_block_sizes,
                cuda_device_buffer_async<std::byte>& scratch_space,
                cuda_device_buffer_async<void*>& compressed_block_ptrs,
                cuda_device_buffer_async<size_t>& compressed_block_sizes,
                cuda_device_buffer_async<nvcompStatus_t>& block_statuses,
                const compression_options& options
            ) const override
            {
                const auto status = nvcompBatchedSnappyCompressAsync(
                    uncompressed_block_ptrs.get(),
                    uncompressed_block_sizes.get(),
                    block_size,
                    num_blocks,
                    scratch_space.get_raw(),
                    scratch_space.bytes(),
                    compressed_block_ptrs.get(),
                    compressed_block_sizes.get(),
                    std::get<nvcompBatchedSnappyCompressOpts_t>(options),
                    block_statuses.get(),
                    cudaStreamPerThread
                );

                if (status != nvcompStatus_t::nvcompSuccess)
                {
                    throw std::runtime_error(
                        std::format(
                            "snappy: nvcompBatchedSnappyCompressAsync failed to launch due to nvcomp error: '{}'",
                            util::status_t_to_string(status)
                        )
                    );
                }
            };

            void decompression_impl(
                size_t num_blocks,
                const cuda_device_buffer_async<void*>& compressed_block_ptrs,
                const cuda_device_buffer_async<size_t>& compressed_block_sizes,
                cuda_device_buffer_async<std::byte>& scratch_space,
                cuda_device_buffer_async<void*>& uncompressed_block_ptrs,
                cuda_device_buffer_async<size_t>& uncompressed_block_sizes,
                cuda_device_buffer_async<nvcompStatus_t>& block_statuses,
                const decompression_options& options
            ) const override
            {
                auto block_sizes_out = cuda::make_device_buffer<size_t>(num_blocks);

                const auto status = nvcompBatchedSnappyDecompressAsync(
                    compressed_block_ptrs.get(),
                    compressed_block_sizes.get(),
                    uncompressed_block_sizes.get(),
                    block_sizes_out.get(),
                    num_blocks,
                    scratch_space.get_raw(),
                    scratch_space.size,
                    uncompressed_block_ptrs.get(),
                    std::get<nvcompBatchedSnappyDecompressOpts_t>(options),
                    block_statuses.get(),
                    cudaStreamPerThread
                );

                if (status != nvcompStatus_t::nvcompSuccess)
                {
                    throw std::runtime_error(
                        std::format(
                            "snappy: nvcompBatchedSnappyDecompressAsync failed to launch due to nvcomp error: '{}'",
                            util::status_t_to_string(status)
                        )
                    );
                }
            }
        };
    } // namespace cuda
} // namespace NAMESPACE_COMPRESSED_IMAGE
