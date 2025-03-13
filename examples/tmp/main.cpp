#include <execution>
#include <algorithm>

#include <compressed/image.h>


auto main() -> int
{
	auto image = compressed::image<float>::read("C:/Users/emild/Desktop/Camera_FULL_FRONT_HDR_Night.exr");
	image.print_statistics();
	
	auto [r, g, b] = image.channels_ref("R", "G", "B");
	for (auto chunk : r)
	{
		std::for_each(std::execution::par_unseq, chunk.begin(), chunk.end(), [](float& value)
			{
				value += .1f;
			});
	}

	image.print_statistics();
}