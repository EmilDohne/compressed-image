#include <pybind11/pybind11.h>

#include "bind_channel.h"
#include "bind_image.h"
#include "bind_enums.h"



PYBIND11_MODULE(compressed_image, m) 
{
	compressed_py::bind_enums(m);
	compressed_py::bind_compressed_channel(m);
	compressed_py::bind_compressed_image(m);
}