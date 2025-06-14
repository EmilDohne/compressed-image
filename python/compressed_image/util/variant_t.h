#pragma once

#include <format>
#include <stdexcept>
#include <concepts>
#include <variant>

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace compressed_py
{

	/// \brief Dispatch a function by dtype allowing you to capture a c++ type from it.
	/// 
	/// Usage:
	/// dispatch_by_dtype(dtype, [&](auto tag) -> dynamic_channel 
	/// {
	///		using T = decltype(tag);
	/// }
	template <typename Func>
	auto dispatch_by_dtype(const py::dtype& dtype, Func&& f)
	{
		if (dtype.is(py::dtype::of<float>())) 
		{
			return std::invoke(std::forward<Func>(f), float{});
		}
		else if (dtype.is(py::dtype::of<double>())) 
		{
			return std::invoke(std::forward<Func>(f), double{});
		}
		else if (dtype.is(py::dtype::of<uint8_t>())) 
		{
			return std::invoke(std::forward<Func>(f), uint8_t{});
		}
		else if (dtype.is(py::dtype::of<int8_t>())) 
		{
			return std::invoke(std::forward<Func>(f), int8_t{});
		}
		else if (dtype.is(py::dtype::of<uint16_t>())) 
		{
			return std::invoke(std::forward<Func>(f), uint16_t{});
		}
		else if (dtype.is(py::dtype::of<int16_t>())) 
		{
			return std::invoke(std::forward<Func>(f), int16_t{});
		}
		else if (dtype.is(py::dtype::of<uint32_t>())) 
		{
			return std::invoke(std::forward<Func>(f), uint32_t{});
		}
		else if (dtype.is(py::dtype::of<int32_t>())) 
		{
			return std::invoke(std::forward<Func>(f), int32_t{});
		}
		else 
		{
			throw std::runtime_error(std::format(
				"Unsupported dtype: kind='{}', itemsize={}.\n"
				"Supported types are: np.float32, np.float64, np.uint8, np.int8, "
				"np.uint16, np.int16, np.uint32, np.int32.",
				dtype.kind(), dtype.itemsize()
			));
		}
	}


	template <typename T>
	concept np_bitdepth = requires std::is_same_v<T, float> ||
		std::is_same_v<T, double>   ||
		std::is_same_v<T, uint8_t>  ||
		std::is_same_v<T, int8_t>   ||
		std::is_same_v<T, uint16_t> ||
		std::is_same_v<T, int16_t>  ||
		std::is_same_v<T, uint32_t> ||
		std::is_same_v<T, int32_t>;

	// variants for our type-erased(ish) classes we expose on the python side.
	template <template<typename> class Class>
	using variant_t = std::variant<
		std::shared_ptr<Class<float>>,		// python equivalent: np.float32
		std::shared_ptr<Class<double>>,		// python equivalent: np.float64
		std::shared_ptr<Class<uint8_t>>,	// python equivalent: np.uint8
		std::shared_ptr<Class<int8_t>>,		// python equivalent: np.int8
		std::shared_ptr<Class<uint16_t>>,	// python equivalent: np.uint16
		std::shared_ptr<Class<int16_t>>,	// python equivalent: np.int16
		std::shared_ptr<Class<uint32_t>>,	// python equivalent: np.uint32
		std::shared_ptr<Class<int32_t>>		// python equivalent: np.int32
	>;


	/// \brief std::variant based type-erasure base-class to bind templated classes into python
	///
	/// Allows us to have type-erasure on the python side (accessing the type through the `dtype` property)
	/// while not having to use `void*` for the underlying data giving us strong typing on the cpp side.
	template <template<typename> class Class>
	class base_variant_class
	{
	protected:
		variant_t<Class> m_ClassVariant;

	public:

		/// \brief Default-initialize a uint8_t `Class` ensuring we always have a valid ptr
		base_variant_class()
		{
			m_ClassVariant = std::make_shared<Class<uint8_t>>();
		}

		base_variant_class(variant_t<Class> variant)
			: m_ClassVariant(variant) 
		{
		}

		py::dtype dtype() const 
		{
			return std::visit([](const auto& ptr)
				{
				using T = typename std::decay_t<decltype(*ptr)>::value_type;
				return py::dtype::of<T>();
				}, m_ClassVariant);
		}
	};

}