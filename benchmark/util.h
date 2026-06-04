#pragma once

#include <filesystem>
#include <string_view>

#include <OpenImageIO/imageio.h>

static const std::filesystem::path s_images_path = std::filesystem::current_path() / "images";


/// Helper to get clean string representations of types for benchmark naming
template <typename T>
constexpr std::string_view type_name()
{
    if constexpr (std::is_same_v<T, uint8_t>) return "uint8_t";
    else if constexpr (std::is_same_v<T, uint16_t>) return "uint16_t";
    else if constexpr (std::is_same_v<T, uint32_t>) return "uint32_t";
    else if constexpr (std::is_same_v<T, half> || std::is_same_v<T, Imath::half>) return "half";
    else if constexpr (std::is_same_v<T, float>) return "float";
    else return "unknown_t";
}

/// Iterate all the images in ./images/ and return them
inline std::vector<std::filesystem::path> get_images()
{
    std::vector<std::filesystem::path> result{};
    if (!std::filesystem::exists(s_images_path)) return result;

    for (const auto& entry : std::filesystem::directory_iterator(s_images_path))
    {
        if (std::filesystem::is_regular_file(entry))
        {
            const auto ext = entry.path().extension();
            if (ext == ".exr" || ext == ".jpg" || ext == ".png" ||
                ext == ".tiff" || ext == ".tif" || ext == ".bmp" ||
                ext == ".webp" || ext == ".tga" || ext == ".hdr")
            {
                result.push_back(entry.path());
            }
        }
    }
    return result;
}
