/*
Entry point for the various compressors of nvcomp
*/

#pragma once

#include <variant>
#include <cstddef>
#include <vector>

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

#include "compressed/cuda/memory.h"
#include "compressed/cuda/enums.h"


namespace NAMESPACE_COMPRESSED_IMAGE
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
			nvcompBatchedZstdCompressOpts_t,
		>;

		/// \brief decompression options for the various nvcomp compressors.
		using decompression_options = std::variant<
			nvcompBatchedLZ4DecompressOpts_t,
			nvcompBatchedCascadedDecompressOpts_t,
			nvcompBatchedDeflateDecompressOpts_t,
			nvcompBatchedGdeflateDecompressOpts_t,
			nvcompBatchedGzipDecompressOpts_t,
			nvcompBatchedSnappyDecompressOpts_t,
			nvcompBatchedZstdDecompressOpts_t,
		>;


		/// A single compressed chunk holding either device or host memory, depending on the storage location
		struct compressed_chunk
		{
			std::variant<util::default_init_vector<std::byte>, cuda_device_ptr_async<std::byte>> data;
			cuda::enums::storage_location location{};
			NAMESPACE_COMPRESSED_IMAGE::enums::codec codec{};
		};
		

		namespace detail
		{

			/// \brief base cuda-based compressor base that provides utility functions for the various compressor implementations
			///
			/// Note: we use blosc2 terminology for a lot of these calls i.e. what nvcomp may call 'chunks' we call blocks
			///		  as each unit we are compressing is already a chunk. This gives us the same 3D data structure where 
			///		  we cascade from channel -> chunks -> blocks. Unlike with blosc2 the blocks here are transparent to us
			///		  but we hide them from the call site
			template <typename T>
			struct compressor
			{

				virtual ~compressor() = default;

				/// \brief compress the given cpu-based buffer into either device or host memory
				///
				/// \param data			The data to compress
				/// \param location		Where to store the compressed block, in host memory or in device memory. Typically it 
				///						is more efficient to store in device memory as this allows us to not pay the cost of 
				///						transferring the compressed data back to the host. However, this must be balanced with
				///						the overall storage capacities of the device.
				/// \param options		The compression options, if none are passed we choose sensible defaults for the given 
				///						compressor. This must be valid for the current compression algorithm
				/// \param block_size	The block size to use for compression, will split the input `data` into this amount
				///						of blocks.
				/// 
				/// \throws std::bad_variant_access if the compression_options are not correct for the current compressor.
				compressed_chunk compress(
					std::vector<T> data,
					cuda::enums::storage_location location,
					std::optional<compression_options> options = std::nullopt,
					size_t block_size = s_default_blocksize
				) const
				{
					// Ensure the block size does not exceed the max the compressor allows for.
					block_size = this->fit_block_size(block_size);

					// Allocate the buffer for `data` on the device and memcpy over. Note that we do this using the 
					// asynchronous API giving us two benefits:
					// - Other threads (streams) may do other operations in-between
					// - It uses the cudaMallocAsync/cudaMallocFree under the hood which is memory-pooled speeding up
					//   the allocations tremendously as we do not have to 
					auto device_data = make_device_buffer_async<T>(data.size());
					cuda_api::instance().memcpy_async(
						device_data.get_raw(),
						static_cast<void*>(data.data()),
						device_data.bytes(),
						cudaMemcpyHostToDevice
					);
				};

				/// \brief The max block size allowed for a given compressor. Implementation defined.
				virtual size_t max_block_size() const noexcept = 0;

				/// \brief Fits the given `block_size` to be <= what the compressor allows.
				size_t fit_block_size(size_t block_size) const noexcept
				{
					return std::min(block_size, this->max_block_size());
				};

			private:

				/// \brief Compute a vector of all the block sizes of the uncompressed data
				///
				/// \param num_bytes The total uncompressed bytes
				/// \param block_size The block size, already fitted to fit within 
				std::vector<size_t> generate_block_sizes(size_t num_bytes, size_t block_size, size_t num_blocks)
				{
					std::vector<size_t> out(num_blocks, block_size);
					if (out.size() > 0)
					{
						out[out.size() - 1] = num_bytes - (block_size * (num_blocks - 1));
					}
					return out;
				}

				/// \brief Retrieve the number of temporary device bytes needed for the compression/decompression procedure
				///
				/// \param block_size The block size of one of the sub-streams
				/// \param num_blocks The total number of blocks
				virtual size_t get_temp_bytes(
					size_t block_size,
					size_t num_blocks,
					std::variant<compression_options, decompression_options> options
				) = 0;


			};

		}




	} // namespace cuda

} // namespace NAMESPACE_COMPRESSED_IMAGE