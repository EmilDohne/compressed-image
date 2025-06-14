#pragma once

#include <vector>
#include <variant>

#include "../util/variant_t.h"
#include "compressed/image.h"
#include "dynamic_channel.h"

#include "pybind11_json/pybind11_json.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#include <py_img_util/image.h>

namespace py = pybind11;

namespace compressed_py
{

	/// Dynamic, variant-based wrapper around a compressed::image allowing us to bind only a single type
	/// to python exposing the data in a more pythonic dynamic fashion that is more akin to a np.ndarray
	struct dynamic_image : public base_variant_class<compressed::image>
	{

		dynamic_image(
			py::dtype dtype,
			std::vector<py::array> channels,
			size_t width,
			size_t height,
			std::vector<std::string> channel_names = {},
			compressed::enums::codec compression_codec = compressed::enums::codec::lz4,
			size_t compression_level = 9,
			size_t block_size = compressed::s_default_blocksize,
			size_t chunk_size = compressed::s_default_chunksize
		)
		{
			dispatch_by_dtype(dtype, [&](auto tag) -> dynamic_image
				{
					using T = decltype(tag);
					static_assert(np_bitdepth<T>, "Unsupported type passed to full");

					std::vector<std::span<const T>> data_span;
					for (auto& channel : channels)
					{
						data_span.push_back(py_img_util::from_py_array(py_img_util::tag::view, channel, width, height));
					}

					auto image = std::make_shared<compressed::image<T>>(
						data_span,
						width,
						height,
						channel_names,
						compression_codec,
						compression_level,
						block_size,
						chunk_size
					);
					base_variant_class::m_ClassVariant = image;
				});
		}

		static std::shared_ptr<dynamic_image> read(
			py::dtype dtype,
			std::filesystem::path filepath,
			int subimage,
			compressed::enums::codec compression_codec = compressed::enums::codec::lz4,
			size_t compression_level = 9,
			size_t block_size = compressed::s_default_blocksize,
			size_t chunk_size = compressed::s_default_chunksize
		)
		{
			return dispatch_by_dtype(dtype, [&](auto tag) -> std::shared_ptr<dynamic_image>
				{
					using T = decltype(tag);
					static_assert(np_bitdepth<T>, "Unsupported type passed to full");

					auto image = compressed::image<T>::read(filepath, subimage, compression_codec, compression_level, block_size, chunk_size);
					auto image_ptr = std::shared_ptr<compressed::image<T>>(&image);
					return std::make_shared<dynamic_image>(image_ptr);
				});
		}

		static std::shared_ptr<dynamic_image> read(
			py::dtype dtype,
			std::filesystem::path filepath,
			int subimage,
			std::vector<int> channel_indices,
			compressed::enums::codec compression_codec = compressed::enums::codec::lz4,
			size_t compression_level = 9,
			size_t block_size = compressed::s_default_blocksize,
			size_t chunk_size = compressed::s_default_chunksize
		)
		{
			return dispatch_by_dtype(dtype, [&](auto tag) -> std::shared_ptr<dynamic_image>
				{
					using T = decltype(tag);
					static_assert(np_bitdepth<T>, "Unsupported type passed to full");

					// Initialize the OIIO primitives
					auto input_ptr = OIIO::ImageInput::open(filepath);
					if (!input_ptr)
					{
						throw std::invalid_argument(std::format("File {} does not exist on disk", filepath.string()));
					}

					auto image = compressed::image<T>::read(input_ptr, channel_indices, subimage, compression_codec, compression_level, block_size, chunk_size);
					auto image_ptr = std::shared_ptr<compressed::image<T>>(&image);
					return std::make_shared<dynamic_image>(image_ptr);
				});
		}

		static std::shared_ptr<dynamic_image> read(
			py::dtype dtype,
			std::filesystem::path filepath,
			int subimage,
			std::vector<std::string> channel_names,
			compressed::enums::codec compression_codec = compressed::enums::codec::lz4,
			size_t compression_level = 9,
			size_t block_size = compressed::s_default_blocksize,
			size_t chunk_size = compressed::s_default_chunksize
		)
		{
			return dispatch_by_dtype(dtype, [&](auto tag) -> std::shared_ptr<dynamic_image>
				{
					using T = decltype(tag);
					static_assert(np_bitdepth<T>, "Unsupported type passed to full");

					// Initialize the OIIO primitives
					auto input_ptr = OIIO::ImageInput::open(filepath);
					if (!input_ptr)
					{
						throw std::invalid_argument(std::format("File {} does not exist on disk", filepath.string()));
					}

					auto image = compressed::image<T>::read(input_ptr, channel_names, subimage, compression_codec, compression_level, block_size, chunk_size);
					auto image_ptr = std::shared_ptr<compressed::image<T>>(&image);
					return std::make_shared<dynamic_image>(image_ptr);
				});
		}

		void add_channel(
			py::array data,
			size_t width,
			size_t height,
			std::optional<std::string> name = std::nullopt,
			compressed::enums::codec compression_codec = compressed::enums::codec::lz4,
			size_t compression_level = 5
		)
		{
			std::visit([](auto&& img_ptr)
				{
					img_ptr->add_channel(
						py_img_util::from_py_array(py_img_util::tag::view, data, width, height)
						width,
						height,
						name,
						compression_codec,
						compression_level
					);
				}, base_variant_class::m_ClassVariant
			);
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
			return std::visit([](auto&& img_ptr)
				{
					using T = typename std::decay_t<decltype(*ch_ptr)>::value_type;

					auto& channel = img_ptr->channel(index);
					return std::shared_ptr<compressed::channel<T>>(&channel);
				}, base_variant_class::m_ClassVariant
			);
		}

		std::shared_ptr<dynamic_channel> channel(const std::string_view name)
		{
			return std::visit([](auto&& img_ptr)
				{
					using T = typename std::decay_t<decltype(*img_ptr)>::value_type;

					auto& channel = img_ptr->channel(name);
					auto ptr = std::shared_ptr<compressed::channel<T>>(&channel)
					return std::make_shared<dynamic_channel>(ptr);
				}, base_variant_class::m_ClassVariant
			);
		}

		std::vector<std::shared_ptr<dynamic_channel>> channels()
		{
			return std::visit([](auto&& img_ptr)
				{
					using T = typename std::decay_t<decltype(*img_ptr)>::value_type;

					auto& channels = img_ptr->channels();
					std::vector<std::shared_ptr<dynamic_channel>> out_channels;

					for (auto& channel : channels)
					{
						auto ptr = std::shared_ptr<compressed::channel<T>>(&channel);
						out_channels.push_back(std::make_shared<dynamic_channel>(ptr));
					}

					return out_channels;
				}, base_variant_class::m_ClassVariant
			);
		}

		std::vector<py::array> get_decompressed()
		{
			return std::visit([](auto&& img_ptr)
				{
					std::vector<py::array> out;

					// Since this->channels already converts to a `dynamic_channel` we can right away
					// get the py::array from it
					for (auto& channel_ptr : this->channels())
					{
						out.push_back(channel_ptr->get_decompressed());
					}

					return out
				}, base_variant_class::m_ClassVariant);
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