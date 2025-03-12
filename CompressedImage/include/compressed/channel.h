#pragma once

#include <array>
#include <vector>
#include <span>
#include <memory>
#include <optional>
#include <limits>
#include <execution>

#include "blosc2.h"
#include "json.hpp"

#include "macros.h"
#include "fwd.h"
#include "blosc2_wrapper.h"
#include "constants.h"

#include "iterators/iterator.h"

using json_ordered = nlohmann::ordered_json;


namespace NAMESPACE_COMPRESSED_IMAGE
{

	template <typename T, size_t BlockSize = s_default_blocksize, size_t ChunkSize = s_default_chunksize>
	struct channel
	{
		static constexpr size_t block_size = BlockSize;
		static constexpr size_t chunk_size = ChunkSize;

		channel(
			const std::span<const T> data,
			size_t width,
			size_t height,
			codec compression_codec = codec::lz4,
			uint8_t compression_level = 5
		) : m_Width(width), m_Height(height), m_Codec(compression_codec)
		{
			if (data.size() != width * height)
			{
				throw std::runtime_error(
					std::format(
						"Invalid channel data passed. Expected its size to match up to width * height ({} * {}) which would be {:L}." \
						" Instead received {:L}",
						width, height, width * height, data.size()
					)
				);
			}

			// c-blosc2 chunks can at most be 2 gigabytes so the set chunk size should not exceed this.
			static_assert(ChunkSize < std::numeric_limits<int32_t>::max());
			static_assert(BlockSize < ChunkSize);

			// Initialize our Super-Chunk first as that is required for the compression and decompression contexts. 
			// The cparams and dparams don't matter as we use the verbose blosc2_schunk_append_chunk and blosc2_decompress_ctx
			blosc2_storage storage = { .cparams = &BLOSC2_CPARAMS_DEFAULTS , .dparams = &BLOSC2_DPARAMS_DEFAULTS };
			m_Schunk = blosc2_schunk_new(&storage);

			m_CompressionContext = create_compression_context(std::thread::hardware_concurrency());
			m_DecompressionContext = create_decompression_context(std::thread::hardware_concurrency());

			auto compression_span = std::span<const std::byte>(data.data(), data.size() * sizeof(T));
			this->compress(compression_span);
		}

		iterator begin()
		{
			return iterator(m_Schunk.get(), m_CompressionContext.get(), m_DecompressionContext.get(), 0, m_Width, m_Height);
		}

		iterator end()
		{
			return iterator(m_Schunk.get(), m_CompressionContext.get(), m_DecompressionContext.get(), m_Schunk->nchunks, m_Width, m_Height);
		}

		/// Retrieve a view to the compression context. In most cases users will not have to modify this.
		blosc2::context_raw_ptr compression_context() { return m_CompressionContext.get(); }

		/// Retrieve a view to the decompression context. In most cases users will not have to modify this.
		blosc2::context_raw_ptr decompression_context() { return m_DecompressionContext.get(); }


		/// Update the number of threads used internally by c-blosc2 for compression and decompression.
		/// This is automatically set when iterating through the images with compressed::for_each for example
		/// by specifying the compression codec.
		void update_nthreads(size_t nthreads)
		{
			m_CompressionContext = create_compression_context(nthreads);
			m_DecompressionContext = create_decompression_context(nthreads);
		}

		size_t width() { return m_Width; }
		size_t height() { return m_Height; }
		codec compression() { return m_Schunk; }

		size_t compressed_size() { return m_Schunk->cbytes; }
		size_t uncompressed_size() { return m_Schunk->nbytes; }

	private:
		/// The storage for the internal data, stored contiguously in a compressed data format
		blosc2::schunk_ptr m_Schunk = nullptr;
		codec m_Codec = codec::lz4;

		/// We store a compression and decompression context here to allow us to reuse them rather than having
		/// to reinitialize them on launch. May be nullptr;
		blosc2::context_ptr m_CompressionContext = nullptr;
		blosc2::context_ptr m_DecompressionContext = nullptr;

		size_t m_Width = 1;
		size_t m_Height = 1;
	private:

		/// Maps a codec enum into its blosc2 representation.
		///
		/// \param compcode the compression codec to get
		/// 
		/// \returns The mapped enum as uint8_t since blosc expects it that way
		constexpr uint8_t codec_to_blosc2(enums::codec compcode)
		{
			if constexpr (compcode == enums::codec::blosclz)
			{
				return static_cast<uint8_t>(BLOSC_BLOSCLZ);
			}
			else if constexpr (compcode == enums::codec::lz4)
			{
				return static_cast<uint8_t>(BLOSC_LZ4);
			}
			else if constexpr (compcode == enums::codec::lz4hc)
			{
				return static_cast<uint8_t>(BLOSC_LZ4HC);
			}
			else if constexpr (compcode == enums::codec::zlib)
			{
				return static_cast<uint8_t>(BLOSC_ZLIB);
			}
			else if constexpr (compcode == enums::codec::zstd)
			{
				return static_cast<uint8_t>(BLOSC_ZSTD);
			}
			return BLOSC_BLOSCLZ;
		}

		/// Create a blosc2 compression context with the given number of threads.
		blosc2::context_ptr create_compression_context(size_t nthreads)
		{
			auto cparams = create_blosc2_cparams(nthreads);
			return blosc2_create_cctx(cparams);
		}

		blosc2_cparams create_blosc2_cparams(size_t nthreads, uint8_t compression_level)
		{
			auto cparams = BLOSC2_CPARAMS_DEFAULTS;
			cparams.blocksize = BlockSize;
			cparams.typesize = sizeof(T);
			cparams.splitmode = BLOSC_AUTO_SPLIT;
			cparams.clevel = compression_level
			cparams.nthreads = nthreads;
			cparams.schunk = m_Schunk.get();
			cparams.compcode = codec_to_blosc2(m_Codec);

			return cparams;
		}

		/// Create a blosc2 decompression context with the given number of threads.
		blosc2::context_ptr create_decompression_context(size_t nthreads)
		{
			auto dparams = BLOSC2_DPARAMS_DEFAULTS;
			dparams.schunk = m_Schunk.get();
			dparams.nthreads = nthreads;

			return blosc2_create_dctx(dparams);
		}

		/// Get the minimum size needed to store the compressed data.
		constexpr size_t min_compressed_size()
		{
			return chunk_size + BLOSC2_MAX_OVERHEAD;
		}

		/// Get the minimum size needed to store the decompressed data.
		constexpr size_t min_decompressed_size()
		{
			return chunk_size;
		}

		/// Compress the image data into m_Schunk. Has to be called after initialization of m_Schunk
		/// as well as after the construction of m_CompressionContext.
		void compress(const std::span<const std::byte> data)
		{
			size_t orig_byte_size = data.size();
			size_t num_chunks = std::ceil(orig_byte_size / ChunkSize);

			// Allocate the chunk once for all of our compression routines.
			std::vector<std::byte> chunk(min_compressed_size());
			
			size_t base_offset = 0;
			for (auto index : std::views::iota(0, num_chunks))
			{
				// Get the size of the section to compress. If this is the last section
				// it may not align directly to the chunk boundary so we must compress less
				size_t section_size = ChunkSize;
				if (base_offset + section_size > orig_byte_size)
				{
					section_size = orig_byte_size - base_offset;
				}

				// Get a subspan of the current chunk to compress
				std::span<const std::byte> subspan(data.data() + base_offset, section_size);

				// We need to tell c-blosc2 that we want to insert at the end
				m_Schunk->current_nchunk = m_Schunk->nchunks;
				// Compress the context into a chunk which we then insert into blosc2
				// we check in the constructor that our chunk size does not exceed int32_t max.
				const auto cbytes = blosc2_compress_ctx(
					m_CompressionContext.get(), 
					static_cast<void*>(subspan.data()), 
					static_cast<int32_t>(subspan.size()), 
					static_cast<void*>(chunk.data()),
					static_cast<int32_t>(chunk.size())
				);

				if (cbytes < 0)
				{
					throw std::runtime_error("Error while compressing blosc2 chunk");
				}

				blosc2_schunk_append_chunk(m_Schunk.get(), chunk.data(), true);

				// Increment our base offset to move onto the next chunk
				base_offset += section_size;
			}
		}
	};

} // NAMESPACE_COMPRESSED_IMAGE