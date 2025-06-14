#include <pybind11/pybind11.h>

#include "bind_channel.h"
#include "bind_image.h"
#include "bind_enums.h"



PYBIND11_MODULE(compressed, m) 
{
	compressed_py::bind_codec_enum(m);
}