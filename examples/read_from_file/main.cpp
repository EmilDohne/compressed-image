
#include <string>
#include <filesystem>
#include <algorithm>
#include <execution>

#include <compressed/image.h>
#include <compressed/ranges.h>


auto main() -> int
{
	std::string name = "uv_grid_2048x2048.jpg";
	auto path = std::filesystem::current_path() / "images" / name;

	// Read the image in chunks without having to load the whole file into memory, this is roughly as fast 
	// or only slightly slower than reading the image data raw through OpenImageIO while taking only a fraction
	// of the memory.
	auto image = compressed::image<uint8_t>::read(path);

	// Get references to the channels as std::tuple<compressed::channel ...>
	auto [r, g, b] = image.channels("R", "G", "B");

	// Now iterate all of them together, first by their chunks, then by their pixels within the chunks.
	// If not all channels are the same size we will iterate to the lowest common denominator!
	for (auto [chunk_r, chunk_g, chunk_b] : compressed::ranges::zip(r, g, b))
	{
		auto chunk_gen = compressed::ranges::zip(chunk_r, chunk_g, chunk_b);
		std::for_each(std::execution::par_unseq, chunk_gen.begin(), chunk_gen.end(), [](auto pixels)
			{
				auto& [r_pixel, g_pixel, b_pixel] = pixels;
				r_pixel = 12;
				g_pixel = 12;
				b_pixel = 12;
			});
	}
}