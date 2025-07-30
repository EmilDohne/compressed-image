
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <span>
#include <algorithm>
#include <execution>
#include <Imath/half.h>

#include <compressed/image.h>
#include <compressed/ranges.h>


auto main() -> int
{
	std::string name = "multilayer_1920x1080.exr";
	auto path = std::filesystem::current_path() / "images" / name;
	
    // Read the image while performing a postprocess on the individual chunks as they are being read before compressing
    // them. This allows us to perform e.g. color space conversions directly without having to pay the cost later of 
    // de- and re-compressing.
    auto image = compressed::image<float>::read(
        path, // image path
        []([[maybe_unused]] size_t channel_idx, std::span<float> chunk)
        {
            std::for_each(std::execution::par_unseq, chunk.begin(), chunk.end(), [](float& pixel)
            {
                pixel = std::pow(pixel, static_cast<float>(1.0f / 2.2f));
            });
        }, // postprocess
        0, // subimage
        compressed::enums::codec::lz4 // compression codec
        );

    // Now that the image is read we can e.g. modify the channels, gather some statistics about the image etc.


    // You may have already noticed that you may not always know ahead of time what color space an image is in before 
    // you actually read it. We provide the `read_oiio_metadata` function allowing you to gather this info before reading
    auto metadata = compressed::image<float>::read_oiio_metadata(path);

    // Use the special 'oiio::ColorSpace' attribute to get the information about the images colorspace. Now that we have
    // this we can use that to make an informed decision during our postprocess
    auto image_space = metadata.at("oiio:ColorSpace");

    std::cout << "Metadata: " << std::setw(4) << metadata << std::endl;
    std::cout << "Color space: " << image_space << std::endl;
}