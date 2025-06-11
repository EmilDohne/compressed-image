#pragma once

#include "../util/variant_t.h"
#include "compressed/channel.h"


namespace compressed_py
{

	struct dynamic_channel : public base_variant_class<compressed::channel>
	{
		using base_variant_class::base_variant_class; // inherit constructors
		
		/// Returns the shape of the channel as (height, width).
		std::tuple<size_t, size_t> shape() const
		{
			return std::visit([](auto&& ch)
				{
					return std::make_tuple(ch.height(), ch.width());
				}, base_variant_class::class_variant
			);
		}

		/// Returns the size in bytes of the compressed data.
		size_t compressed_size() const
		{
			return std::visit([](auto&& ch)
				{
					return ch.compressed_size();
				}, base_variant_class::class_variant
			);
		}

		/// Returns the size in elements of the uncompressed data.
		size_t uncompressed_size() const
		{
			return std::visit([](auto&& ch)
				{
					return ch.uncompressed_size();
				}, base_variant_class::class_variant
			);
		}

		/// Returns the number of chunks in the compressed channel.
		size_t num_chunks() const
		{
			return std::visit([](auto&& ch)
				{
					return ch.num_chunks();
				}, base_variant_class::class_variant
			);
		}

		/// Returns the block size used for compression.
		size_t block_size() const
		{
			return std::visit([](auto&& ch)
				{
					return ch.block_size();
				}, base_variant_class::class_variant
			);
		}

		/// Returns the chunk size used for compression.
		size_t chunk_size() const
		{
			return std::visit([](auto&& ch)
				{
					return ch.chunk_size();
				}, base_variant_class::class_variant
			);
		}

		/// Returns the size of a specific chunk by index.
		/// \param chunk_index Index of the chunk to query.
		size_t chunk_size(size_t chunk_index) const
		{
			return std::visit([chunk_index](auto&& ch)
				{
					return ch.chunk_size(chunk_index);
				}, base_variant_class::class_variant
			);
		}

		/// Copies decompressed chunk data into the provided buffer.
		/// \param buffer Span to store the chunk data.
		/// \param chunk_idx Index of the chunk to retrieve.
		py::array get_chunk(size_t chunk_idx) const
		{
			std::visit([&](auto&& ch)
				{
					ch.get_chunk(buffer, chunk_idx);
				}, base_variant_class::class_variant
			);
		}

		void set_chunk(py::array array, size_t chunk_idx)
		{
			std::visit([&](auto&& ch) 
				{
				using T = typename std::decay_t<decltype(ch)>::value_type;

				}, base_variant_class::class_variant
			);
		}

		std::vector<typename std::decay_t<decltype(std::get<0>(class_variant))>::value_type> get_decompressed() const
		{
			return std::visit([](auto&& ch)
				{
					return ch.get_decompressed();
				}, base_variant_class::class_variant
			);
		}

		void update_nthreads(size_t nthreads, size_t block_size = compressed::s_default_blocksize)
		{
			std::visit([=](auto&& ch)
				{
					ch.update_nthreads(nthreads, block_size);
				}, base_variant_class::class_variant
			);
		}

		enums::codec compression() const noexcept
		{
			return std::visit([](auto&& ch)
				{
					return ch.compression();
				}, base_variant_class::class_variant
			);
		}

		size_t compression_level() const noexcept
		{
			return std::visit([](auto&& ch)
				{
					return ch.compression_level();
				}, base_variant_class::class_variant
			);
		}
	};

} // compressed_py