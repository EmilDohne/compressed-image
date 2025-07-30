
#include <string>
#include <filesystem>
#include <format>

#include <compressed/image.h>
#include <compressed/channel.h>


auto main() -> int
{
	std::string name = "uv_grid_2048x2048.jpg";
	auto path = std::filesystem::current_path() / "images" / name;

	// Read the image in chunks without having to load the whole file into memory, this is roughly as fast 
	// or only slightly slower than reading the image data raw through OpenImageIO while taking only a fraction
	// of the memory.
	auto image = compressed::image<uint8_t>::read(path);

	// Now that we have the image, we might want to add another channel into the mix, (note that block and chunk size
	// must be identical!):
	image.add_channel(
		compressed::channel<uint8_t>::full(
			image.width(),
			image.height(),
			static_cast<uint8_t>(255),
			compressed::enums::codec::zstd, // compression codec, channels within an image may have different compression codecs!
			5,	// compression level, this may be different across channels in the image
			image.block_size(), // block size, this must match the rest of the image as otherwise we will throw an exception!
			image.chunk_size()  // chunk size, this must match the rest of the image as otherwise we will throw an exception!
		), 
		"Z" // channel name
	);

	// The compressed_image api doesn't validate what kind of channels you add into the image, so if you e.g.
	// add the channel 'R' twice that is perfectly valid and accepted.

	// If instead you wish to remove a channel you can call either:
	image.remove_channel("R"); // or pass index 0

	// or:

	auto g_channel = image.extract_channel("G");

	// If you intend to do something with the channel post-removal.
}