#pragma once

#include <memory>

#include "macros.h"
#include "enums.h"

#include "blosc2.h"


namespace NAMESPACE_COMPRESSED_IMAGE
{

	namespace blosc2
	{
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
		uint8_t codec_to_blosc2(enums::codec compcode)
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
		enums::codec blosc2_to_codec(uint8_t compcode)
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

		/// Create a blosc2 compression context with the given number of threads.
		template <typename T, size_t BlockSize>
		blosc2::context_ptr create_compression_context(schunk_ptr& schunk, size_t nthreads)
		{
			auto cparams = create_blosc2_cparams(schunk, nthreads);
			return blosc2::context_ptr(blosc2_create_cctx(cparams));
		}

		template <typename T, size_t BlockSize>
		blosc2_cparams create_blosc2_cparams(schunk_ptr& schunk, size_t nthreads)
		{
			auto cparams = BLOSC2_CPARAMS_DEFAULTS;
			cparams.blocksize = BlockSize;
			cparams.typesize = sizeof(T);
			cparams.splitmode = BLOSC_AUTO_SPLIT;
			cparams.clevel = m_CompressionLevel;
			cparams.nthreads = nthreads;
			cparams.schunk = schunk.get();
			cparams.compcode = codec_to_blosc2(m_Codec);

			return cparams;
		}

		/// Create a blosc2 decompression context with the given number of threads.
		blosc2::context_ptr create_decompression_context(schunk_ptr& schunk, size_t nthreads)
		{
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