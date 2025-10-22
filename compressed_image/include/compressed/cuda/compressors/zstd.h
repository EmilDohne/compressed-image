#pragma once

#include "compressed/macros.h"

#include "compressed/cuda/compressors/base.h"
#include "compressed/cuda/compressors/util.h"

namespace NAMESPACE_COMPRESSED_IMAGE
{

	namespace cuda
	{

		template <typename T>
		struct zstd_compressor final : public detail::compressor<T>
		{

			NAMESPACE_COMPRESSED_IMAGE::enums::codec codec() const noexcept
			{
				return NAMESPACE_COMPRESSED_IMAGE::enums::codec::zstd_gpu;
			};

			compression_options default_compression_opts() const noexcept
			{
			};

			decompression_options default_decompression_opts() const noexcept
			{
			};

			size_t max_block_size() const noexcept
			{
			};

		private:

			size_t get_temp_bytes(
				size_t block_size,
				size_t num_blocks,
				std::variant<compression_options, decompression_options> options
			)
			{
			}


			size_t block_max_compressed_size(size_t block_size, compression_options& options)
			{
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
			}

		};


	} // namespace cuda

} // namespace NAMESPACE_COMPRESSED_IMAGE