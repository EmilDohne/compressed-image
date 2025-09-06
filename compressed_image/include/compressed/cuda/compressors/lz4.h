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
					nvcompBatchedLZ4CompressGetTempSizeAsync(
						num_blocks,
						block_size,
						std::get<nvcompBatchedLZ4CompressOpts_t>(std::get<compression_options>(options)),
						&temp_bytes,
						block_size * num_blocks
					);
					return temp_bytes;
				}

			};

			virtual size_t block_max_compressed_size(size_t block_size, compression_options& options) = 0;


			virtual void compression_impl(
				size_t block_size,
				size_t num_blocks,
				const cuda_device_buffer_async<void>& uncompressed_block_ptrs,
				const cuda_device_buffer_async<size_t>& uncompressed_block_sizes,
				cuda_device_buffer_async<std::byte>& scratch_space,
				cuda_device_buffer_async<void>& compressed_block_ptrs,
				cuda_device_buffer_async<size_t>& compressed_block_sizes,
				const compression_options& options
			) = 0;

			virtual void decompression_impl(
				size_t block_size,
				size_t num_blocks,
				const cuda_device_buffer_async<void>& compressed_block_ptrs,
				const cuda_device_buffer_async<size_t>& compressed_block_sizes,
				cuda_device_buffer_async<std::byte>& scratch_space,
				cuda_device_buffer_async<void>& uncompressed_block_ptrs,
				cuda_device_buffer_async<size_t>& uncompressed_block_sizes,
				const decompression_options& options
			) = 0;

		};


	} // namespace cuda

} // namespace NAMESPACE_COMPRESSED_IMAGE