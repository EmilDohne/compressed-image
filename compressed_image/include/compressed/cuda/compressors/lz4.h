#pragma once

#include "compressed/macros.h"

#include "compressed/cuda/compressors/base.h"
#include "compressed/cuda/compressors/util.h"

namespace NAMESPACE_COMPRESSED_IMAGE
{

	namespace cuda
	{

		template <typename T>
		struct lz4_compressor final : public detail::compressor<T>
		{

			NAMESPACE_COMPRESSED_IMAGE::enums::codec codec() const noexcept
			{
				return NAMESPACE_COMPRESSED_IMAGE::enums::codec::lz4_gpu;
			};

			compression_options default_compression_opts() const noexcept
			{
				auto opts = nvcompBatchedLZ4CompressDefaultOpts;
				opts.data_type = util::to_nvcomp_type<T>();
				return opts;
			};

			decompression_options default_decompression_opts() const noexcept
			{
				return nvcompBatchedLZ4DecompressDefaultOpts;
			};

			size_t max_block_size() const noexcept
			{
				return nvcompLZ4CompressionMaxAllowedChunkSize;
			};

		private:

			size_t get_temp_bytes(
				size_t block_size,
				size_t num_blocks,
				std::variant<compression_options, decompression_options> options
			)
			{
				if (std::holds_alternative<compression_options>(options))
				{
					size_t temp_bytes{};
					auto status = nvcompBatchedLZ4CompressGetTempSizeAsync(
						num_blocks,
						block_size,
						std::get<nvcompBatchedLZ4CompressOpts_t>(std::get<compression_options>(options)),
						&temp_bytes,
						block_size * num_blocks
					);

					if (status != nvcompStatus_t::nvcompSuccess)
					{
						throw std::runtime_error(
							std::format(
								"lz4: Unable to retrieve the scratch bytes required for compression due to nvcomp error: '{}'", 
								util::status_t_to_string(status)
							)
						);
					}

					return temp_bytes;
				}

				size_t temp_bytes{};
				auto status = nvcompBatchedLZ4DecompressGetTempSizeAsync(
					num_blocks,
					block_size,
					std::get<nvcompBatchedLZ4DecompressOpts_t>(std::get<decompression_options>(options)),
					&temp_bytes,
					block_size * num_blocks
				);

				if (status != nvcompStatus_t::nvcompSuccess)
				{
					throw std::runtime_error(
						std::format(
							"lz4: Unable to retrieve the scratch bytes required for decompression due to nvcomp error: '{}'", 
							util::status_t_to_string(status)
						)
					);
				}

				return temp_bytes;

			}


			size_t block_max_compressed_size(size_t block_size, compression_options& options)
			{
				size_t max_bytes = 0;
				auto status = nvcompBatchedLZ4CompressGetMaxOutputChunkSize(
					block_size, 
					std::get<nvcompBatchedLZ4CompressOpts_t>(options), 
					&max_bytes
				);

				if (status != nvcompStatus_t::nvcompSuccess)
				{
					throw std::runtime_error(
						std::format(
							"lz4: Unable to retrieve the maximum compressed size for a block due to nvcomp error: '{}'", 
							util::status_t_to_string(status)
						)
					);
				}

				return max_bytes;
			}


			void compression_impl(
				size_t block_size,
				size_t num_blocks,
				const cuda_device_buffer_async<void>& uncompressed_block_ptrs,
				const cuda_device_buffer_async<size_t>& uncompressed_block_sizes,
				cuda_device_buffer_async<std::byte>& scratch_space,
				cuda_device_buffer_async<void>& compressed_block_ptrs,
				cuda_device_buffer_async<size_t>& compressed_block_sizes,
				cuda_device_buffer_async<nvcompStatus_t>& block_statuses,
				const compression_options& options
			) const 
			{
				auto status = nvcompBatchedLZ4CompressAsync(
					uncompressed_block_ptrs.data(),
					uncompressed_block_sizes.data(),
					block_size,
					num_blocks,
					scratch_space.data(),
					scratch_space.size(),
					compressed_block_ptrs.data(),
					compressed_block_sizes.data(),
					std::get<nvcompBatchedLZ4CompressOpts_t>(options),
					block_statuses.data(),
					cudaStreamPerThread);

				if (status != nvcompStatus_t::nvcompSuccess)
				{
					throw std::runtime_error(
						std::format(
							"lz4: nvcompBatchedLZ4CompressAsync failed to launch due to nvcomp error: '{}'",
							util::status_t_to_string(status)
						)
					);
				}
			};

			virtual void decompression_impl(
				size_t block_size,
				size_t num_blocks,
				const cuda_device_buffer_async<void>& compressed_block_ptrs,
				const cuda_device_buffer_async<size_t>& compressed_block_sizes,
				cuda_device_buffer_async<std::byte>& scratch_space,
				cuda_device_buffer_async<void>& uncompressed_block_ptrs,
				cuda_device_buffer_async<size_t>& uncompressed_block_sizes,
				cuda_device_buffer_async<nvcompStatus_t>& block_statuses,
				const decompression_options& options
			) const 
			{
				auto status = nvcompBatchedLZ4DecompressAsync(
					compressed_block_ptrs.data(),
					compressed_block_sizes.data()
					uncompressed_block_sizes.data(),
					num_blocks,
					scratch_space.data(),
					scratch_space.size(),
					uncompressed_block_ptrs.data(),
					std::get<nvcompBatchedLZ4DecompressOpts_t>(options),
					block_statuses.data(),
					cudaStreamPerThread);

				if (status != nvcompStatus_t::nvcompSuccess)
				{
					throw std::runtime_error(
						std::format(
							"lz4: nvcompBatchedLZ4DecompressAsync failed to launch due to nvcomp error: '{}'",
							util::status_t_to_string(status)
						)
					);
				}
			}

		};


	} // namespace cuda

} // namespace NAMESPACE_COMPRESSED_IMAGE