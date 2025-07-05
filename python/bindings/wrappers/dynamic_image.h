#pragma once

#include <vector>
#include <variant>

#include "util/variant_t.h"
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
		using base_variant_class::base_variant_class; // inherit constructors

		dynamic_image(
			const py::object& dtype_,
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
			// This allows to take e.g. np.uint8 or 'uint8' as dtype rather than only allowing for an instantiated 
			// numpy.dtype
			auto dtype = py::dtype::from_args(dtype_);
			dispatch_by_dtype(dtype, [&](auto tag) -> void
				{
					using T = decltype(tag);
					static_assert(np_bitdepth<T>, "Unsupported type passed to full");

					std::vector<std::span<const T>> data_span;
					for (auto& channel : channels)
					{
						py::array_t<T> typed_array = channel.cast<py::array_t<T>>();
						data_span.push_back(py_img_util::from_py_array(py_img_util::tag::view{}, typed_array, width, height));
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
			const py::object& dtype_,
			std::string filepath,
			int subimage,
			compressed::enums::codec compression_codec = compressed::enums::codec::lz4,
			size_t compression_level = 9,
			size_t block_size = compressed::s_default_blocksize,
			size_t chunk_size = compressed::s_default_chunksize
		)
		{
			// This allows to take e.g. np.uint8 or 'uint8' as dtype rather than only allowing for an instantiated 
			// numpy.dtype
			auto dtype = py::dtype::from_args(dtype_);
			return dispatch_by_dtype(dtype, [&](auto tag) -> std::shared_ptr<dynamic_image>
				{
					using T = decltype(tag);
					static_assert(np_bitdepth<T>, "Unsupported type passed to read");

					auto image_ptr = std::make_shared<compressed::image<T>>(
						compressed::image<T>::read(filepath, subimage, compression_codec, compression_level, block_size, chunk_size)
					);
					return std::make_shared<dynamic_image>(std::move(image_ptr));
				});
		}

		static std::shared_ptr<dynamic_image> read(
			const py::object& dtype_,
			std::string filepath,
			int subimage,
			std::vector<int> channel_indices,
			compressed::enums::codec compression_codec = compressed::enums::codec::lz4,
			size_t compression_level = 9,
			size_t block_size = compressed::s_default_blocksize,
			size_t chunk_size = compressed::s_default_chunksize
		)
		{
			// This allows to take e.g. np.uint8 or 'uint8' as dtype rather than only allowing for an instantiated 
			// numpy.dtype
			auto dtype = py::dtype::from_args(dtype_);
			return dispatch_by_dtype(dtype, [&](auto tag) -> std::shared_ptr<dynamic_image>
				{
					using T = decltype(tag);
					static_assert(np_bitdepth<T>, "Unsupported type passed to read");

					// Initialize the OIIO primitives
					auto input_ptr = OIIO::ImageInput::open(filepath);
					if (!input_ptr)
					{
						throw std::invalid_argument(std::format("File {} does not exist on disk", filepath));
					}

					auto image = compressed::image<T>::read(std::move(input_ptr), channel_indices, subimage, compression_codec, compression_level, block_size, chunk_size);
					auto image_ptr = std::make_shared<compressed::image<T>>(std::move(image));
					return std::make_shared<dynamic_image>(variant_t<compressed::image>(image_ptr));
				});
		}

		static std::shared_ptr<dynamic_image> read(
			const py::object& dtype_,
			std::string filepath,
			int subimage,
			std::vector<std::string> channel_names,
			compressed::enums::codec compression_codec = compressed::enums::codec::lz4,
			size_t compression_level = 9,
			size_t block_size = compressed::s_default_blocksize,
			size_t chunk_size = compressed::s_default_chunksize
		)
		{
			// This allows to take e.g. np.uint8 or 'uint8' as dtype rather than only allowing for an instantiated 
			// numpy.dtype
			auto dtype = py::dtype::from_args(dtype_);
			return dispatch_by_dtype(dtype, [&](auto tag) -> std::shared_ptr<dynamic_image>
				{
					using T = decltype(tag);
					static_assert(np_bitdepth<T>, "Unsupported type passed to read");

					// Initialize the OIIO primitives
					auto input_ptr = OIIO::ImageInput::open(filepath);
					if (!input_ptr)
					{
						throw std::invalid_argument(std::format("File {} does not exist on disk", filepath));
					}

					auto image = compressed::image<T>::read(std::move(input_ptr), channel_names, subimage, compression_codec, compression_level, block_size, chunk_size);
					auto image_ptr = std::make_shared<compressed::image<T>>(std::move(image));
					return std::make_shared<dynamic_image>(variant_t<compressed::image>(image_ptr));
				});
		}

		void add_channel(
			py::array data,
			size_t width,
			size_t height,
			std::optional<std::string> name = std::nullopt,
			compressed::enums::codec compression_codec = compressed::enums::codec::lz4,
			uint8_t compression_level = 5
		)
		{
			std::visit([&](auto&& img_ptr)
				{
					using T = typename std::decay_t<decltype(*img_ptr)>::value_type;

					if (!py::isinstance<py::array_t<T>>(data))
					{
						throw std::invalid_argument("Array must have dtype matching image element type.");
					}
					// Validate dimensions
					if (data.ndim() != 2)
					{
						throw std::invalid_argument("Array must be 2-dimensional.");
					}

					py::array_t<T> typed_array = data.cast<py::array_t<T>>();
					img_ptr->add_channel(
						py_img_util::from_py_array(py_img_util::tag::view{}, typed_array, width, height),
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
			return std::visit([&](auto&& img_ptr)
				{
					using T = typename std::decay_t<decltype(*img_ptr)>::value_type;

					auto& ch = img_ptr->channel(index);
					auto aliasing_ptr = std::shared_ptr<compressed::channel<T>>(img_ptr, &ch);
					return std::make_shared<dynamic_channel>(aliasing_ptr);
				}, base_variant_class::m_ClassVariant
			);
		}

		std::shared_ptr<dynamic_channel> channel(const std::string_view name)
		{
			return std::visit([&](auto&& img_ptr)
				{
					using T = typename std::decay_t<decltype(*img_ptr)>::value_type;

					// Here we alias 
					auto& ch = img_ptr->channel(name);
					auto aliasing_ptr = std::shared_ptr<compressed::channel<T>>(img_ptr, &ch);
					return std::make_shared<dynamic_channel>(aliasing_ptr);
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
						auto aliasing_ptr = std::shared_ptr<compressed::channel<T>>(img_ptr, &channel);
						out_channels.push_back(std::make_shared<dynamic_channel>(aliasing_ptr));
					}

					return out_channels;
				}, base_variant_class::m_ClassVariant
			);
		}

		std::vector<py::array> get_decompressed()
		{
			std::vector<py::array> out;

			// Since this->channels already converts to a `dynamic_channel` we can right away
			// get the py::array from it
			for (auto& channel_ptr : this->channels())
			{
				out.push_back(channel_ptr->get_decompressed());
			}

			return out;
		}


		size_t get_channel_index(std::string channelname)
		{
			return std::visit([&](auto&& img_ptr)
				{
					return img_ptr->get_channel_offset(channelname);
				}, base_variant_class::m_ClassVariant
			);
		}

		std::vector<std::string> channel_names() const
		{
			return std::visit([&](auto&& img_ptr)
				{
					return img_ptr->channelnames();
				}, base_variant_class::m_ClassVariant
			);
		}

		void channel_names(std::vector<std::string> _channelnames)
		{
			std::visit([&](auto&& img_ptr)
				{
					img_ptr->channelnames(_channelnames);
				}, base_variant_class::m_ClassVariant
			);
		}

		/// Returns the shape of the image as (nchannels, height, width).
		std::tuple<size_t, size_t, size_t> shape() const noexcept
		{
			return std::make_tuple(this->num_channels(), this->height(), this->width());
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
			std::visit([&](auto&& img_ptr)
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

		void metadata(const nlohmann::json& _metadata) noexcept
		{
			std::visit([&](auto&& img_ptr)
				{
					img_ptr->metadata(_metadata);
				}, base_variant_class::m_ClassVariant
			);
		}

		nlohmann::json metadata() noexcept
		{
			return std::visit([](auto&& img_ptr)
				{
					return img_ptr->metadata();
				}, base_variant_class::m_ClassVariant
			);
		}

	};

} // compressed_py