#pragma once

#include <vector>
#include <variant>

#include "util/npy_half.h"
#include "util/variant_t.h"
#include "compressed/channel.h"
#include "compressed/util.h"

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#include <py_img_util/image.h>

namespace py = pybind11;

namespace compressed_py
{

	/// Dynamic, variant-based wrapper around a compressed::channel allowing us to bind only a single type
	/// to python exposing the data in a more pythonic dynamic fashion that is more akin to a np.ndarray
	struct dynamic_channel : public base_variant_class<compressed::channel>
	{

		using base_variant_class::base_variant_class; // inherit constructors

		dynamic_channel(
			py::array data,
			size_t width,
			size_t height,
			compressed::enums::codec compression_codec = compressed::enums::codec::lz4,
			uint8_t compression_level = 9,
			size_t block_size = compressed::s_default_blocksize,
			size_t chunk_size = compressed::s_default_chunksize
		)
		{
			dispatch_by_dtype(data.dtype(), [&](auto tag)
				{
					using T = decltype(tag);
					static_assert(np_bitdepth<T>, "Unsupported type passed to full");

					py::array_t<T> typed_array = data.cast<py::array_t<T>>();

					auto channel = std::make_shared<compressed::channel<T>>(
						py_img_util::from_py_array(py_img_util::tag::view{}, typed_array, width, height),
						width,
						height,
						compression_codec,
						compression_level,
						block_size,
						chunk_size
					);
					base_variant_class::m_ClassVariant = channel;
				});
		}

		static std::shared_ptr<dynamic_channel> full(
			const py::object& dtype_,
			py::object fill_value,
			size_t width,
			size_t height,
			compressed::enums::codec compression_codec = compressed::enums::codec::lz4,
			uint8_t compression_level = 9,
			size_t block_size = compressed::s_default_blocksize,
			size_t chunk_size = compressed::s_default_chunksize
		)
		{
			// This allows to take e.g. np.uint8 or 'uint8' as dtype rather than only allowing for an instantiated 
			// numpy.dtype
			auto dtype = py::dtype::from_args(dtype_);
			return dispatch_by_dtype(dtype, [&](auto tag) -> std::shared_ptr<dynamic_channel> 
				{
					using T = decltype(tag);
					static_assert(np_bitdepth<T>, "Unsupported type passed to full");

					T value{};
					try
					{
						// Attempt to cast the fill_value to the correct target type
						value = fill_value.cast<T>();
					}
					catch (const py::cast_error&)
					{
						throw std::runtime_error("Could not convert fill_value to the target dtype.");
					}

					auto channel = std::make_shared<compressed::channel<T>>(
						compressed::channel<T>::full(width, height, value, compression_codec, compression_level, block_size, chunk_size)
					);
					return std::make_shared<dynamic_channel>(channel);
				});
		}

		static std::shared_ptr<dynamic_channel> full_like(const dynamic_channel& other, py::object fill_value)
		{
			return std::visit([&](auto&& ch_ptr)
				{
					using T = typename std::decay_t<decltype(*ch_ptr)>::value_type;
					T value{};
					try 
					{
						// Attempt to cast the fill_value to the correct target type
						value = fill_value.cast<T>();
					}
					catch (const py::cast_error&) 
					{
						throw std::runtime_error("Could not convert fill_value to the target dtype.");
					}

					auto channel = std::make_shared<compressed::channel<T>>(
						compressed::channel<T>::full_like(*ch_ptr, value)
					);
					return std::make_shared<dynamic_channel>(channel);
				}, other.m_ClassVariant);
		}

		static std::shared_ptr<dynamic_channel> zeros(
			const py::object& dtype_,
			size_t width,
			size_t height,
			compressed::enums::codec compression_codec = compressed::enums::codec::lz4,
			uint8_t compression_level = 9,
			size_t block_size = compressed::s_default_blocksize,
			size_t chunk_size = compressed::s_default_chunksize)
		{
			// This allows to take e.g. np.uint8 or 'uint8' as dtype rather than only allowing for an instantiated 
			// numpy.dtype
			auto dtype = py::dtype::from_args(dtype_);
			return dispatch_by_dtype(dtype, [&](auto tag) -> std::shared_ptr<dynamic_channel> 
				{
					using T = decltype(tag);
					static_assert(np_bitdepth<T>, "Unsupported type passed to zeros");

					auto channel = std::make_shared<compressed::channel<T>>(
						compressed::channel<T>::zeros(width, height, compression_codec, compression_level, block_size, chunk_size)
					);
					return std::make_shared<dynamic_channel>(channel);
				});
		}

		static std::shared_ptr<dynamic_channel> zeros_like(const std::shared_ptr<dynamic_channel>& other)
		{
			return std::visit([](auto&& ch_ptr)
				{
					using T = typename std::decay_t<decltype(*ch_ptr)>::value_type;
					auto channel = std::make_shared<compressed::channel<T>>(
						compressed::channel<T>::zeros_like(*ch_ptr)
					);
					return std::make_shared<dynamic_channel>(channel);
				}, other->m_ClassVariant);
		}

		/// Returns the shape of the channel as (height, width).
		std::tuple<size_t, size_t> shape() const 
		{
			return std::visit([](auto&& ch_ptr) 
				{
					return std::make_tuple(ch_ptr->height(), ch_ptr->width());
				}, m_ClassVariant);
		}

		size_t width() const 
		{
			return std::visit([](auto&& ch_ptr) 
				{
					return ch_ptr->width();
				}, m_ClassVariant);
		}

		size_t height() const 
		{
			return std::visit([](auto&& ch_ptr) 
				{
					return ch_ptr->height();
				}, m_ClassVariant);
		}

		/// Returns the size in bytes of the compressed data.
		size_t compressed_bytes() const
		{
			return std::visit([](auto&& ch_ptr)
				{
					return ch_ptr->compressed_bytes();
				}, base_variant_class::m_ClassVariant
			);
		}

		/// Returns the size in elements of the uncompressed data.
		size_t uncompressed_size() const
		{
			return std::visit([](auto&& ch_ptr)
				{
					return ch_ptr->uncompressed_size();
				}, base_variant_class::m_ClassVariant
			);
		}

		/// Returns the number of chunks in the compressed channel.
		size_t num_chunks() const
		{
			return std::visit([](auto&& ch_ptr)
				{
					return ch_ptr->num_chunks();
				}, base_variant_class::m_ClassVariant
			);
		}

		/// Returns the block size used for compression.
		size_t block_size() const
		{
			return std::visit([](auto&& ch_ptr)
				{
					return ch_ptr->block_size();
				}, base_variant_class::m_ClassVariant
			);
		}

		/// Returns the chunk size used for compression.
		size_t chunk_size() const
		{
			return std::visit([](auto&& ch_ptr)
				{
					return ch_ptr->chunk_size();
				}, base_variant_class::m_ClassVariant
			);
		}

		size_t chunk_elems() const
		{
			return std::visit([](auto&& ch_ptr)
				{
					return ch_ptr->chunk_elems();
				}, base_variant_class::m_ClassVariant
			);
		}

		/// Returns the size of a specific chunk by index.
		/// \param chunk_index Index of the chunk to query.
		size_t chunk_size(size_t chunk_index) const
		{
			return std::visit([&](auto&& ch_ptr)
				{
					return ch_ptr->chunk_size(chunk_index);
				}, base_variant_class::m_ClassVariant
			);
		}

		size_t chunk_elems(size_t chunk_index) const
		{
			return std::visit([&](auto&& ch_ptr)
				{
					return ch_ptr->chunk_elems(chunk_index);
				}, base_variant_class::m_ClassVariant
			);
		}

		// Fill the passed array with the chunk data.
		void get_chunk(size_t chunk_idx, py::array& array) const
		{
			std::visit([&](auto&& ch_ptr) -> void
				{
					using T = typename std::decay_t<decltype(*ch_ptr)>::value_type;

					// Validate dtype
					if (!py::isinstance<py::array_t<T>>(array))
					{
						throw std::invalid_argument("Array must have dtype matching channel element type.");
					}

					// Validate dimensions
					if (array.ndim() != 1)
					{
						throw std::invalid_argument("Array must be 1-dimensional.");
					}

					// Validate size
					if (array.shape(0) != static_cast<py::ssize_t>(ch_ptr->chunk_elems(chunk_idx)))
					{
						throw std::invalid_argument("Array length does not match number of chunk elements.");
					}

					// Get a mutable span and fill it
					auto buf = static_cast<T*>(array.mutable_data());
					std::span<T> buffer(buf, ch_ptr->chunk_elems(chunk_idx));

					ch_ptr->get_chunk(buffer, chunk_idx);

				}, base_variant_class::m_ClassVariant);
		}

		/// Copies decompressed chunk data into the provided buffer.
		/// \param chunk_idx Index of the chunk to retrieve.
		py::array get_chunk(size_t chunk_idx) const
		{
			return std::visit([&](auto&& ch_ptr) -> py::array
				{
					using T = typename std::decay_t<decltype(*ch_ptr)>::value_type;
					compressed::util::default_init_vector<T> buffer(this->chunk_elems(chunk_idx));
					ch_ptr->get_chunk(std::span<T>(buffer), chunk_idx);

					// Use the size, ptr overload of array_t. Since array_t is an extension of py::array we can
					// implicitly cast back down.
					return py::array_t<T>(static_cast<py::ssize_t>(buffer.size()), buffer.data());
				}, base_variant_class::m_ClassVariant);
		}

		void set_chunk(size_t chunk_idx, py::array array)
		{
			std::visit([&](auto&& ch_ptr)
				{
					using T = typename std::decay_t<decltype(*ch_ptr)>::value_type;

					// Request buffer info
					py::buffer_info info = array.request();
					if (info.ndim != 1)
					{
						throw py::value_error("set_chunk requires a 1D numpy array");
					}

					if (!info.item_type_is_equivalent_to<T>())
					{
						throw py::value_error(
							std::format("set_chunk requires numpy array of struct type '{}', instead got '{}'",
								std::string(py::format_descriptor<T>::format()),
								std::string(info.format)
							)
						);
					}

					std::span<T> buffer(static_cast<T*>(info.ptr), info.size);

					ch_ptr->set_chunk(buffer, chunk_idx);
				}, base_variant_class::m_ClassVariant);
		}

		py::array get_decompressed() const
		{
			return std::visit([](auto&& ch_ptr) -> py::array
				{
					using T = typename std::decay_t<decltype(*ch_ptr)>::value_type;
					auto decompressed = ch_ptr->get_decompressed();

					// This will handle converting it into a 2d numpy array.
					return py_img_util::to_py_array(std::move(decompressed), ch_ptr->width(), ch_ptr->height());
				}, base_variant_class::m_ClassVariant);
		}

		void update_nthreads(size_t nthreads, size_t block_size = compressed::s_default_blocksize)
		{
			std::visit([=](auto&& ch_ptr)
				{
					ch_ptr->update_nthreads(nthreads, block_size);
				}, base_variant_class::m_ClassVariant
			);
		}

		compressed::enums::codec compression() const noexcept
		{
			return std::visit([](auto&& ch_ptr)
				{
					return ch_ptr->compression();
				}, base_variant_class::m_ClassVariant
			);
		}

		size_t compression_level() const noexcept
		{
			return std::visit([](auto&& ch_ptr)
				{
					return ch_ptr->compression_level();
				}, base_variant_class::m_ClassVariant
			);
		}
	};


} // compressed_py