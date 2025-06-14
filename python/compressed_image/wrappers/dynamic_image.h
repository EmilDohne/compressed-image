#pragma once

#include "../util/variant_t.h"
#include "compressed/image.h"
#include "dynamic_channel.h"

#include "pybind11_json/pybind11_json.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#include <py_img_util/image.h>

namespace compressed_py
{

	/// Dynamic, variant-based wrapper around a compressed::image allowing us to bind only a single type
	/// to python exposing the data in a more pythonic dynamic fashion that is more akin to a np.ndarray
	struct dynamic_image : public base_variant_class<compressed::image>
	{

		dynamic_image(
			std::vector<py::array> channels,
			size_t width,
			size_t height,
			std::vector<std::string> channel_names = {},
			enums::codec compression_codec = enums::codec::lz4,
			size_t compression_level = 9,
			size_t block_size = s_default_blocksize,
			size_t chunk_size = s_default_chunksize
		)
		{

		}

		static std::shared_ptr<dynamic_image> read(
			std::filesystem::path filepath,
			int subimage,
			enums::codec compression_codec = enums::codec::lz4,
			size_t compression_level = 9,
			size_t block_size = s_default_blocksize,
			size_t chunk_size = s_default_chunksize
		)
		{
		}

		static std::shared_ptr<dynamic_image> read(
			std::filesystem::path filepath,
			int subimage,
			std::vector<int> channel_indices,
			enums::codec compression_codec = enums::codec::lz4,
			size_t compression_level = 9,
			size_t block_size = s_default_blocksize,
			size_t chunk_size = s_default_chunksize
		)
		{
		}

		static std::shared_ptr<dynamic_image> read(
			std::filesystem::path filepath,
			int subimage,
			std::vector<std::string> channel_indices,
			enums::codec compression_codec = enums::codec::lz4,
			size_t compression_level = 9,
			size_t block_size = s_default_blocksize,
			size_t chunk_size = s_default_chunksize
		)
		{
		}

		void add_channel(
			py::array data,
			size_t width,
			size_t height,
			std::optional<std::string> name = std::nullopt,
			enums::codec compression_codec = enums::codec::lz4,
			size_t compression_level = 5
		)
		{
		}

		void print_statistics()
		{
			std::visit([](auto&& img_ptr)
				{
					img_ptr->print_statistics();
				}, base_variant_class::m_ClassVariant
			);
		}

		double compression_ratio() const noexcept
		{
			return std::visit([](auto&& img_ptr)
				{
					return img_ptr->compression_ratio();
				}, base_variant_class::m_ClassVariant
			);
		}

		std::shared_ptr<dynamic_channel> channel(size_t index)
		{
		}

		std::shared_ptr<dynamic_channel> channel(const std::string_view name)
		{
		}

		std::vector<std::shared_ptr<dynamic_channel>> channels()
		{
		}

		std::vector<py::array> get_decompressed()
		{
		}

		size_t get_channel_index(std::string channelname)
		{
			return std::visit([](auto&& img_ptr)
				{
					return img_ptr->get_channel_offset();
				}, base_variant_class::m_ClassVariant
			);
		}

		size_t width() const noexcept
		{
			return std::visit([](auto&& img_ptr)
				{
					return img_ptr->width();
				}, base_variant_class::m_ClassVariant
			);
		}

		size_t height() const noexcept
		{
			return std::visit([](auto&& img_ptr)
				{
					return img_ptr->height();
				}, base_variant_class::m_ClassVariant
			);
		}

		size_t num_channels() const noexcept
		{
			return std::visit([](auto&& img_ptr)
				{
					return img_ptr->num_channels();
				}, base_variant_class::m_ClassVariant
			);
		}

		void update_nthreads(size_t nthreads)
		{
			std::visit([](auto&& img_ptr)
				{
					img_ptr->update_nthreads(nthreads);
				}, base_variant_class::m_ClassVariant
			);
		}

		size_t chunk_size() const
		{
			return std::visit([](auto&& img_ptr)
				{
					return img_ptr->chunk_size();
				}, base_variant_class::m_ClassVariant
			);
		}

		void metadata(const json_ordered& _metadata) noexcept
		{
			std::visit([](auto&& img_ptr)
				{
					img_ptr->metadata(_metadata);
				}, base_variant_class::m_ClassVariant
			);
		}

		json_ordered metadata() noexcept
		{
			return std::visit([](auto&& img_ptr)
				{
					return img_ptr->metadata();
				}, base_variant_class::m_ClassVariant
			);
		}

	}