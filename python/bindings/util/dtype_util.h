#pragma once

#include <OpenImageIO/imageio.h>
#include <Imath/half.h>

#include <pybind11/numpy.h>


namespace py = pybind11;

namespace compressed_py
{

    py::dtype from_type_desc(OIIO::TypeDesc desc)
    {
        if (desc.basetype == OIIO::TypeDesc::UINT8)
        {
            return py::dtype::of<uint8_t>();
        }
        else if (desc.basetype == OIIO::TypeDesc::INT8)
        {
            return py::dtype::of<int8_t>();
        }
        else if (desc.basetype == OIIO::TypeDesc::UINT16)
        {
            return py::dtype::of<uint16_t>();
        }
        else if (desc.basetype == OIIO::TypeDesc::INT16)
        {
            return py::dtype::of<int16_t>();
        }
        else if (desc.basetype == OIIO::TypeDesc::UINT32)
        {
            return py::dtype::of<uint32_t>();
        }
        else if (desc.basetype == OIIO::TypeDesc::INT32)
        {
            return py::dtype::of<int32_t>();
        }
        else if (desc.basetype == OIIO::TypeDesc::HALF)
        {
            return py::dtype::of<Imath::half>();
        }
        else if (desc.basetype == OIIO::TypeDesc::FLOAT)
        {
            return py::dtype::of<float>();
        }
        else if (desc.basetype == OIIO::TypeDesc::DOUBLE)
        {
            return py::dtype::of<double>();
        }
        
        throw std::runtime_error(
            std::format(
                "Unhandled OpenImageIO typedesc '{}' encountered while converting to python dtype",
                desc.c_str()
            )
        );
        return py::dtype::of<double>();
    }

} // compressed_py