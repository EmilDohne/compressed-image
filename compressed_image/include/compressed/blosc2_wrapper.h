#pragma once

#include <memory>

#include "macros.h"
#include "enums.h"
#include "detail/scoped_timer.h"

#include "blosc2.h"
#include "blosc2/blosc2-common.h"
#include "blosc2/blosc2-stdio.h"
#include "blosc2/filters-registry.h"


namespace NAMESPACE_COMPRESSED_IMAGE
{

	namespace blosc2
	{

		namespace detail
		{
			static const inline bool g_filters_registered = false;

			/// Initialize filters in c-blosc2. Since we don't have an explicit entry point this needs to be checked on every call to compress and decompress.
			/// May be a no-op if detail::g_filters_registered is true.
			inline void init_filters()
			{
				if (!detail::g_filters_registered)
				{
					register_filters();
				}
			}

		}

		// Custom deleter for blosc2 structs for use in a smart pointer
		template <typename T>
		struct deleter {};

		template <>
		struct deleter<blosc2_schunk>
		{
			void operator()(blosc2_schunk* schunk)
			{
				blosc2_schunk_free(schunk);
			}
		};

		template <>
		struct deleter<blosc2_context>
		{
			void operator()(blosc2_context* context)
			{
				blosc2_free_ctx(context);
			}
		};


		/// Typedef the blosc2 primitives into both smart pointers and as raw ptrs
		typedef std::unique_ptr<blosc2_schunk, deleter<blosc2_schunk>>		schunk_ptr;
		typedef blosc2_schunk*												schunk_raw_ptr;
		typedef void*														chunk_raw_ptr;
		typedef std::unique_ptr<blosc2_context, deleter<blosc2_context>>	context_ptr;
		typedef blosc2_context*												context_raw_ptr;


		
		/// Maps a codec enum into its blosc2 representation.
		///
		/// \param compcode the compression codec to get
		/// 
		/// \returns The mapped enum as uint8_t since blosc expects it that way
		inline uint8_t codec_to_blosc2(enums::codec compcode)
		{
			if (compcode == enums::codec::blosclz)
			{
				return static_cast<uint8_t>(BLOSC_BLOSCLZ);
			}
			else if (compcode == enums::codec::lz4)
			{
				return static_cast<uint8_t>(BLOSC_LZ4);
			}
			else if (compcode == enums::codec::lz4hc)
			{
				return static_cast<uint8_t>(BLOSC_LZ4HC);
			}
			return BLOSC_BLOSCLZ;
		}

		/// Maps a blosc2 compression codec into an enum representation
		///
		/// \param compcode the compression codec to get
		/// 
		/// \returns The mapped enum
		inline enums::codec blosc2_to_codec(uint8_t compcode)
		{
			if (compcode == BLOSC_BLOSCLZ)
			{
				return enums::codec::blosclz;
			}
			else if (compcode == BLOSC_LZ4)
			{
				return  enums::codec::lz4;
			}
			else if (compcode == BLOSC_LZ4HC)
			{
				return enums::codec::lz4hc;
			}
			return enums::codec::blosclz;
		}

		
		/// Compress the `data` into `chunk` using the provided `context`. 
		/// 
		/// This function applies Blosc2 compression to the input `data` and stores the compressed 
		/// result in `chunk`. If compression fails, it throws a `std::runtime_error` with the 
		/// corresponding Blosc2 error code.
		/// 
		/// \tparam T The data type of the input buffer.
		/// \param context A raw pointer to the Blosc2 compression context.
		/// \param data The input data to be compressed, provided as a `std::span<T>`.
		/// \param chunk The output chunk where compressed data will be stored, provided as a `std::span<std::byte>`.
		/// \returns The compressed byte size of the chunk. This size includes a header with metadata, 
		///          which Blosc2 internally uses.
		/// \throws std::runtime_error if compression fails, with the Blosc2 error code.
		template <typename T>
		size_t compress(context_raw_ptr context, std::span<T> data, std::span<std::byte> chunk)
		{
			_COMPRESSED_PROFILE_FUNCTION();
			detail::init_filters();
			const auto cbytes = blosc2_compress_ctx(
				context,
				static_cast<const void*>(data.data()),
				static_cast<int32_t>(data.size() * sizeof(T)),
				static_cast<void*>(chunk.data()),
				static_cast<int32_t>(chunk.size())
			);
			if (cbytes < 0)
			{
				throw std::runtime_error(std::format("Unable to compress context using Blosc2 with error code {}", cbytes));
			}

			return cbytes;
		}
		
		/// Compress the `data` into `chunk` using the provided `context`. 
		/// 
		/// This function applies Blosc2 compression to the input `data` and stores the compressed 
		/// result in `chunk`. If compression fails, it throws a `std::runtime_error` with the 
		/// corresponding Blosc2 error code.
		/// 
		/// \tparam T The data type of the input buffer.
		/// \param context A unique pointer to the Blosc2 compression context.
		/// \param data The input data to be compressed, provided as a `std::span<T>`.
		/// \param chunk The output chunk where compressed data will be stored, provided as a `std::span<std::byte>`.
		/// \returns The compressed byte size of the chunk. This size includes a header with metadata, 
		///          which Blosc2 internally uses.
		/// \throws std::runtime_error if compression fails, with the Blosc2 error code.
		template <typename T>
		size_t compress(context_ptr& context, std::span<T> data, std::span<std::byte> chunk)
		{
			return compress(context.get(), data, chunk);
		}


		/// Decompress a Blosc2 `chunk` into `buffer` using the provided `context`. 
		/// 
		/// This function reverses the Blosc2 compression, restoring the original uncompressed data. 
		/// If decompression fails, it throws a `std::runtime_error` with the corresponding error code.
		/// 
		/// \tparam T The data type of the decompressed output.
		/// \param context A raw pointer to the Blosc2 decompression context.
		/// \param buffer The output buffer where decompressed data will be stored, provided as a `std::span<T>`.
		/// \param chunk The compressed input data to be decompressed, provided as a `std::span<std::byte>`.
		/// \returns The decompressed byte size of the buffer.
		/// \throws std::runtime_error if decompression fails, with the Blosc2 error code.
		template <typename T>
		size_t decompress(context_raw_ptr context, std::span<T> buffer, std::span<std::byte> chunk)
		{
			_COMPRESSED_PROFILE_FUNCTION();
			detail::init_filters();
			int decompressed_size = blosc2_decompress_ctx(
				context,
				static_cast<void*>(chunk.data()),
				std::numeric_limits<int>::max(),
				buffer.data(),
				buffer.size() * sizeof(T)
			);

			if (decompressed_size < 0)
			{
				throw std::runtime_error(std::format("Error code {} while decompressing blosc2 chunk", decompressed_size));
			}
			return decompressed_size;
		}


		/// Decompress a Blosc2 `chunk` into `buffer` using the provided `context`. 
		/// 
		/// This function reverses the Blosc2 compression, restoring the original uncompressed data. 
		/// If decompression fails, it throws a `std::runtime_error` with the corresponding error code.
		/// 
		/// \tparam T The data type of the decompressed output.
		/// \param context A unique pointer to the Blosc2 decompression context.
		/// \param buffer The output buffer where decompressed data will be stored, provided as a `std::span<T>`.
		/// \param chunk The compressed input data to be decompressed, provided as a `std::span<std::byte>`.
		/// \returns The decompressed byte size of the buffer.
		/// \throws std::runtime_error if decompression fails, with the Blosc2 error code.
		template <typename T>
		size_t decompress(context_ptr& context, std::span<T> buffer, std::span<std::byte> chunk)
		{
			return decompress(context.get(), buffer, chunk);
		}

		/// Append the chunk into the super-chunk. The chunk in this case does not need to be refitted as its actual
		/// size since c-blosc will read the size from its header bytes.
		inline size_t append_chunk(schunk_ptr& schunk, std::span<std::byte> chunk)
		{
			detail::init_filters();
			// We don't expose the copy parameter as internally in c-blosc if the chunk was compressed at all (i.e. compressed size < 
			// uncompressed size) the chunk gets realloc'd anyways effectively copying it.
			auto nchunks = blosc2_schunk_append_chunk(
				schunk.get(),
				reinterpret_cast<uint8_t*>(chunk.data()),
				true // copy
			);

			if (nchunks < 0)
			{
				throw std::runtime_error(std::format("Unable to append chunk into super-chunk with the following blosc2 error code {}", nchunks));
			}

			return nchunks;
		}

		/// Create a default schunk with BLOSC2_CPARAMS_DEFAULTS and BLOSC2_DPARAMS_DEFAULTS
		inline blosc2::schunk_ptr create_default_schunk()
		{
			detail::init_filters();
			auto cparams = BLOSC2_CPARAMS_DEFAULTS;
			auto dparams = BLOSC2_DPARAMS_DEFAULTS;
			blosc2_storage storage = { .cparams = &cparams, .dparams = &dparams };
			return blosc2::schunk_ptr(blosc2_schunk_new(&storage));
		}

		/// Create blosc2 compression parameters for the given input.
		template <typename T, size_t BlockSize>
		blosc2_cparams create_blosc2_cparams(schunk_ptr& schunk, size_t nthreads, enums::codec codec, uint8_t compression_level)
		{
			detail::init_filters();
			blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
			cparams.blocksize = BlockSize;
			cparams.typesize = sizeof(T);
			cparams.splitmode = BLOSC_AUTO_SPLIT;
			cparams.clevel = compression_level;
			cparams.nthreads = nthreads;
			cparams.schunk = schunk.get();
			// Shuffle first, then bytedelta
			//cparams.filters[BLOSC2_MAX_FILTERS - 2] = BLOSC_SHUFFLE;
			//cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_FILTER_BYTEDELTA;
			// c-blosc2 documentation mentions that for the BYTEDELTA filter we need to ensure 
			// we set the typesize in the filters_meta. In the tests though its says keeping it 
			// at 0 (which is default) will set it the schunks typesize which is what we want.
			cparams.compcode = codec_to_blosc2(codec);

			return cparams;
		}

		/// Create a blosc2 compression context with the given number of threads.
		template <typename T, size_t BlockSize>
		blosc2::context_ptr create_compression_context(schunk_ptr& schunk, size_t nthreads, enums::codec codec, uint8_t compression_level)
		{
			detail::init_filters();
			auto cparams = create_blosc2_cparams<T, BlockSize>(schunk, nthreads, codec, compression_level);
			return blosc2::context_ptr(blosc2_create_cctx(cparams));
		}

		
		/// Create a blosc2 decompression context with the given number of threads.
		inline blosc2::context_ptr create_decompression_context(schunk_ptr& schunk, size_t nthreads)
		{
			detail::init_filters();
			auto dparams = BLOSC2_DPARAMS_DEFAULTS;
			dparams.schunk = schunk.get();
			dparams.nthreads = nthreads;

			return blosc2::context_ptr(blosc2_create_dctx(dparams));
		}

		/// Get the minimum size needed to store the compressed data.
		template <size_t ChunkSize>
		constexpr size_t min_compressed_size()
		{
			return ChunkSize + BLOSC2_MAX_OVERHEAD;
		}

		/// Get the minimum size needed to store the decompressed data.
		template <size_t ChunkSize>
		constexpr size_t min_decompressed_size()
		{
			return ChunkSize;
		}

	} // namespace blosc2


} // NAMESPACE_COMPRESSED_IMAGE