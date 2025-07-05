#pragma once

#include <Imath/half.h>

#include <pybind11/pybind11.h>
#include <pybind11/cast.h>
#include <pybind11/numpy.h>

namespace pybind11::detail
{

	constexpr int NPY_FLOAT16 = 23;

	// Specialize the npy_format_descriptor for the Imath::half datatype mapping it to the numpy float16
	// datatype (which appears to be 23). Both npy.float16 and Imath::half both have 1 sign bit, 5 exponent
	// bits and 10 mantissa as well as being IEEE 754.
	template <>
	struct npy_format_descriptor<Imath::half> 
	{
		static pybind11::dtype dtype() 
		{
			handle ptr = npy_api::get().PyArray_DescrFromType_(NPY_FLOAT16);
			return reinterpret_borrow<pybind11::dtype>(ptr);
		}
		static std::string format() 
		{
			// following: https://docs.python.org/3/library/struct.html#format-characters
			return "e";
		}
		static constexpr auto name = _("float16");
	};


	template <>
	struct type_caster<Imath::half>
	{
	public:
		PYBIND11_TYPE_CASTER(Imath::half, _("float16"));

		// Python -> C++
		bool load(handle src, bool)
		{
			try
			{
				value = Imath::half(pybind11::cast<float>(src));
				return true;
			}
			catch (...)
			{
				return false;
			}
		}

		// C++ -> Python
		static handle cast(const Imath::half& src, return_value_policy, handle)
		{
			return pybind11::float_(static_cast<float>(src)).release();
		}
	};

} // pybind11::detail