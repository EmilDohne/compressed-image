#include <compressed/image.h>


auto main() -> int
{
	auto image = compressed::image<float>::read("C:/Users/emild/Desktop/Camera_FULL_FRONT_HDR_Night.exr");
	image.print_statistics();
}