#include "doctest.h"

#include <ranges>
#include <string>
#include <cstdint>

#include <OpenImageIO/half.h>

#include <compressed/image.h>
#include <compressed/ranges.h>
#include <compressed/util.h>
#include <compressed/blosc2/wrapper.h>

#include "util.h"


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Read compressed file smaller than one chunk")
{
	std::string name = "uv_grid_2048x2048.jpg";
	auto path = std::filesystem::current_path() / "images" / name;

	auto image = compressed::image<uint8_t>::read(
		path, 
		compressed::enums::codec::lz4, 
		9,
		compressed::s_default_blocksize, 
		compressed::s_default_chunksize * 2
	);
	auto image_data = image.get_decompressed();
	auto image_ref = test_util::read_oiio<uint8_t>(path);


	test_util::compare_images(image_data, image_ref, name);
}

// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Read compressed file and extract channels")
{
	std::string name = "uv_grid_2048x2048.jpg";
	auto path = std::filesystem::current_path() / "images" / name;

	auto image = compressed::image<uint8_t>::read(path);

	std::vector<std::vector<uint8_t>> decompressed;
	for ([[maybe_unused]] auto _: std::views::iota(size_t{ 0 }, image.num_channels()))
	{
		// Since we keep pulling out the channels the indices change back to zero
		auto channel = image.extract_channel(0);
		decompressed.push_back(channel.get_decompressed());
	}
	auto image_ref = test_util::read_oiio<uint8_t>(path);

	// Since we extracted the channels, the number of channels should be zero with the channelnames 
	// also being empty
	CHECK(image.num_channels() == 0);
	CHECK(image.channelnames() == std::vector<std::string>{});

	test_util::compare_images(decompressed, image_ref, name);
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Read compressed file get attributes")
{
	std::string name = "uv_grid_2048x2048.jpg";
	auto path = std::filesystem::current_path() / "images" / name;

	auto image = compressed::image<uint8_t>::read(path);

	CHECK(image.width() == 2048);
	CHECK(image.height() == 2048);
	CHECK(image.num_channels() == 3);
	CHECK(image.channelnames() == std::vector<std::string>{"R", "G", "B"});
	CHECK(image.metadata().size() > 0);
	CHECK(image.chunk_size() == compressed::s_default_chunksize);
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Read compressed file exactly than one chunk")
{
	std::string name = "uv_grid_2048x2048.jpg";
	auto path = std::filesystem::current_path() / "images" / name;

	auto image = compressed::image<uint8_t>::read(path);
	auto image_data = image.get_decompressed();
	auto image_ref = test_util::read_oiio<uint8_t>(path);


	test_util::compare_images(image_data, image_ref, name);
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Read compressed file larger than one chunk")
{
	std::string name = "multilayer_2560x1440.exr";
	auto path = std::filesystem::current_path() / "images" / name;

	auto image = compressed::image<float>::read(
		path, 
		compressed::enums::codec::lz4, 
		9, 
		compressed::s_default_blocksize, 
		compressed::s_default_chunksize / 2
	);
	auto image_data = image.get_decompressed();
	auto image_ref = test_util::read_oiio<float>(path);


	test_util::compare_images(image_data, image_ref, name);
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Read compressed file, subset of channel indices")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&]<typename T>([[maybe_unused]] T type)
		{
			std::string name = "multilayer_2560x1440.exr";
			auto path = std::filesystem::current_path() / "images" / name;
			auto input_ptr = OIIO::ImageInput::open(path.string());

			auto image = compressed::image<T>::read(
				std::move(input_ptr),
				{ 0, 1, 2, 3 },
				compressed::enums::codec::lz4,
				9,
				compressed::s_default_blocksize,
				compressed::s_default_chunksize / 2
			);

			CHECK(image.num_channels() == 4);
			CHECK(image.channelnames() == std::vector<std::string>{"R", "G", "B", "A"});
		});
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Read compressed file, non contiguous channel indices")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&]<typename T>([[maybe_unused]] T type)
		{
			std::string name = "multilayer_2560x1440.exr";
			auto path = std::filesystem::current_path() / "images" / name;
			auto input_ptr = OIIO::ImageInput::open(path.string());

			auto image = compressed::image<T>::read(
				std::move(input_ptr),
				{ 0, 2, 3, 11 },
				compressed::enums::codec::lz4,
				9,
				compressed::s_default_blocksize,
				compressed::s_default_chunksize / 2
			);

			CHECK(image.num_channels() == 4);
			CHECK(image.channelnames() == std::vector<std::string>{ "R", "B", "A", "VRayCryptomatte00.R"});
		});
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Read compressed file, non contiguous channel indices, out of order")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&]<typename T>([[maybe_unused]] T type)
		{
			std::string name = "multilayer_2560x1440.exr";
			auto path = std::filesystem::current_path() / "images" / name;
			auto input_ptr = OIIO::ImageInput::open(path.string());

			auto image = compressed::image<T>::read(
				std::move(input_ptr),
				{ 11, 0, 2, 3 },
				compressed::enums::codec::lz4,
				9,
				compressed::s_default_blocksize,
				compressed::s_default_chunksize / 2
			);

			// Despite us specifying "VRayCryptomatte00.R" first, since it appears later in the channels this should
			// have the same ordering as the file
			CHECK(image.num_channels() == 4);
			CHECK(image.channelnames() == std::vector<std::string>{ "R", "B", "A", "VRayCryptomatte00.R"});
		});
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE(
	"Read compressed file, invalid channel index"
	* doctest::no_breaks(true)
	* doctest::no_output(true)
	* doctest::should_fail(true)
)
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&]<typename T>([[maybe_unused]] T type)
		{
			std::string name = "multilayer_2560x1440.exr";
			auto path = std::filesystem::current_path() / "images" / name;
			auto input_ptr = OIIO::ImageInput::open(path.string());

			// this should fail as this file does not have a 64th channel
			auto image = compressed::image<T>::read(
				std::move(input_ptr),
				{ 0, 1, 64 },
				compressed::enums::codec::lz4,
				9,
				compressed::s_default_blocksize,
				compressed::s_default_chunksize / 2
			);

		});
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Read compressed file, subset of channel names")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&]<typename T>([[maybe_unused]] T type)
		{
			std::string name = "multilayer_2560x1440.exr";
			auto path = std::filesystem::current_path() / "images" / name;
			auto input_ptr = OIIO::ImageInput::open(path.string());

			auto image = compressed::image<T>::read(
				std::move(input_ptr),
				{ "R", "G", "B", "A" },
				compressed::enums::codec::lz4,
				9,
				compressed::s_default_blocksize,
				compressed::s_default_chunksize / 2
			);

			CHECK(image.num_channels() == 4);
			CHECK(image.channelnames() == std::vector<std::string>{"R", "G", "B", "A"});
		});
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Read compressed file, non contiguous channel names")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&]<typename T>([[maybe_unused]] T type)
		{
			std::string name = "multilayer_2560x1440.exr";
			auto path = std::filesystem::current_path() / "images" / name;
			auto input_ptr = OIIO::ImageInput::open(path.string());

			auto image = compressed::image<T>::read(
				std::move(input_ptr),
				{ "R", "B", "A", "VRayCryptomatte00.R" },
				compressed::enums::codec::lz4,
				9,
				compressed::s_default_blocksize,
				compressed::s_default_chunksize / 2
			);

			CHECK(image.num_channels() == 4);
			CHECK(image.channelnames() == std::vector<std::string>{ "R", "B", "A", "VRayCryptomatte00.R"});
		});
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Read compressed file, non contiguous channel names, out of order")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&]<typename T>([[maybe_unused]] T type)
		{
			std::string name = "multilayer_2560x1440.exr";
			auto path = std::filesystem::current_path() / "images" / name;
			auto input_ptr = OIIO::ImageInput::open(path.string());

			auto image = compressed::image<T>::read(
				std::move(input_ptr),
				{ "VRayCryptomatte00.R", "R", "B", "A" },
				compressed::enums::codec::lz4,
				9,
				compressed::s_default_blocksize,
				compressed::s_default_chunksize / 2
			);

			// Despite us specifying "VRayCryptomatte00.R" first, since it appears later in the channels this should
			// have the same ordering as the file
			CHECK(image.num_channels() == 4);
			CHECK(image.channelnames() == std::vector<std::string>{ "R", "B", "A", "VRayCryptomatte00.R"});
		});
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE(
	"Read compressed file, invalid channel name"
	* doctest::no_breaks(true)
	* doctest::no_output(true)
	* doctest::should_fail(true)
)
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&]<typename T>([[maybe_unused]] T type)
		{
			std::string name = "multilayer_2560x1440.exr";
			auto path = std::filesystem::current_path() / "images" / name;
			auto input_ptr = OIIO::ImageInput::open(path.string());

			// this should fail as this file does not have a z channel
			auto image = compressed::image<T>::read(
				std::move(input_ptr),
				{ "R", "G", "Z" },
				compressed::enums::codec::lz4,
				9,
				compressed::s_default_blocksize,
				compressed::s_default_chunksize / 2
			);

		});
}



// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Read compressed file with postprocess, subset of channel names")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&]<typename T>([[maybe_unused]] T type)
		{
			std::string name = "multilayer_2560x1440.exr";
			auto path = std::filesystem::current_path() / "images" / name;
			auto input_ptr = OIIO::ImageInput::open(path.string());

			auto image = compressed::image<T>::read(
				std::move(input_ptr),
				[]([[maybe_unused]] size_t channel_idx, std::span<T> values)
				{
					for (auto& value : values)
					{
						value = static_cast<T>(25);
					}
				},
				{ "R", "G", "B", "A" },
				compressed::enums::codec::lz4,
				9,
				compressed::s_default_blocksize,
				compressed::s_default_chunksize / 2
			);

			CHECK(image.num_channels() == 4);
			CHECK(image.channelnames() == std::vector<std::string>{"R", "G", "B", "A"});

			// Check that our postprocess worked
			auto decompressed = image.get_decompressed();
			for (const auto& channel : decompressed)
			{
				test_util::check_vector_verbose(channel, static_cast<T>(25));
			}
		});
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Read compressed file with postprocess, non contiguous channel names")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&]<typename T>([[maybe_unused]] T type)
		{
			std::string name = "multilayer_2560x1440.exr";
			auto path = std::filesystem::current_path() / "images" / name;
			auto input_ptr = OIIO::ImageInput::open(path.string());

			auto image = compressed::image<T>::read(
				std::move(input_ptr),
				[]([[maybe_unused]] size_t channel_idx, std::span<T> values)
				{
					for (auto& value : values)
					{
						value = static_cast<T>(25);
					}
				},
				{ "R", "B", "A", "VRayCryptomatte00.R" },
				compressed::enums::codec::lz4,
				9,
				compressed::s_default_blocksize,
				compressed::s_default_chunksize / 2
			);

			CHECK(image.num_channels() == 4);
			CHECK(image.channelnames() == std::vector<std::string>{ "R", "B", "A", "VRayCryptomatte00.R"});

			// Check that our postprocess worked
			auto decompressed = image.get_decompressed();
			for (const auto& channel : decompressed)
			{
				test_util::check_vector_verbose(channel, static_cast<T>(25));
			}
		});
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Read compressed file with postprocess, non contiguous channel names, out of order")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&]<typename T>([[maybe_unused]] T type)
		{
			std::string name = "multilayer_2560x1440.exr";
			auto path = std::filesystem::current_path() / "images" / name;
			auto input_ptr = OIIO::ImageInput::open(path.string());

			auto image = compressed::image<T>::read(
				std::move(input_ptr),
				[]([[maybe_unused]] size_t channel_idx, std::span<T> values)
				{
					for (auto& value : values)
					{
						value = static_cast<T>(25);
					}
				},
				{ "VRayCryptomatte00.R", "R", "B", "A" },
				compressed::enums::codec::lz4,
				9,
				compressed::s_default_blocksize,
				compressed::s_default_chunksize / 2
			);

			// Despite us specifying "VRayCryptomatte00.R" first, since it appears later in the channels this should
			// have the same ordering as the file
			CHECK(image.num_channels() == 4);
			CHECK(image.channelnames() == std::vector<std::string>{ "R", "B", "A", "VRayCryptomatte00.R"});

			// Check that our postprocess worked
			auto decompressed = image.get_decompressed();
			for (const auto& channel : decompressed)
			{
				test_util::check_vector_verbose(channel, static_cast<T>(25));
			}
		});
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE(
	"Read compressed file with postprocess, invalid channel name"
	* doctest::no_breaks(true)
	* doctest::no_output(true)
	* doctest::should_fail(true)
)
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&]<typename T>([[maybe_unused]] T type)
		{
			std::string name = "multilayer_2560x1440.exr";
			auto path = std::filesystem::current_path() / "images" / name;
			auto input_ptr = OIIO::ImageInput::open(path.string());

			// this should fail as this file does not have a z channel
			auto image = compressed::image<T>::read(
				std::move(input_ptr),
				[]([[maybe_unused]] size_t channel_idx, std::span<T> values)
				{
					for (auto& value : values)
					{
						value = static_cast<T>(25);
					}
				},
				{ "R", "G", "Z" },
				compressed::enums::codec::lz4,
				9,
				compressed::s_default_blocksize,
				compressed::s_default_chunksize / 2
			);
		});
}




// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Initialize image and iterate parametrized")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&]<typename T>([[maybe_unused]] T type)
		{
			auto channel_r_data = std::vector<T>(128, static_cast<T>(255));

			auto image = compressed::image<T>(
				std::vector<std::vector<T>>{ channel_r_data},
				16,
				8
			);

			SUBCASE("Read")
			{
				auto& r_ref = image.channel(0);
				for (auto chunk : r_ref)
				{
					for (auto& pixel : chunk)
					{
						CHECK(pixel == static_cast<T>(255));
					}
				}
			}

			SUBCASE("Modify")
			{
				auto& r_ref = image.channel(0);
				for (auto chunk : r_ref)
				{
					for (auto& pixel : chunk)
					{
						pixel = static_cast<T>(128);
					}
				}

				auto& r_ref_2 = image.channel(0);
				for (auto chunk_ : r_ref_2)
				{
					for (auto& pixel : chunk_)
					{
						CHECK(pixel == static_cast<T>(128));
					}
				}
			}
		}
	);

}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Zip image channels parametrized")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&]<typename T>([[maybe_unused]] T type)
		{
			auto channel_r_data = std::vector<T>(128, static_cast<T>(255));
			auto channel_g_data = std::vector<T>(128, static_cast<T>(0));
			auto channel_b_data = std::vector<T>(128, static_cast<T>(199));

			auto image = compressed::image<T>(
				std::vector<std::vector<T>>{ channel_r_data, channel_g_data, channel_b_data },
				16,
				8
			);

			auto [r, g, b] = image.channels(0, 1, 2);
			CHECK(r == image.channel(0));
			CHECK(g == image.channel(1));
			CHECK(b == image.channel(2));

			for (auto [r_chunk, g_chunk, b_chunk] : compressed::ranges::zip(r, g, b))
			{
				for (auto [r_pixel, g_pixel, b_pixel] : compressed::ranges::zip(r_chunk, g_chunk, b_chunk))
				{
					CHECK(r_pixel == static_cast<T>(255));
					CHECK(g_pixel == static_cast<T>(0));
					CHECK(b_pixel == static_cast<T>(199));
				}
			}
		}
	);
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Zip image channels equal to chunk size parametrized")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&]<typename T>([[maybe_unused]] T type)
		{
			auto channel_r_data = std::vector<T>(1024, static_cast<T>(255));
			auto channel_g_data = std::vector<T>(1024, static_cast<T>(0));
			auto channel_b_data = std::vector<T>(1024, static_cast<T>(199));

			auto image = compressed::image<T>(
				std::vector<std::vector<T>>{ channel_r_data, channel_g_data, channel_b_data },
				64,
				16,
				{},
				compressed::enums::codec::lz4,
				9,
				256, 
				1024
			);

			auto [r, g, b] = image.channels(0, 1, 2);
			CHECK(r == image.channel(0));
			CHECK(g == image.channel(1));
			CHECK(b == image.channel(2));

			for (auto [r_chunk, g_chunk, b_chunk] : compressed::ranges::zip(r, g, b))
			{
				for (auto [r_pixel, g_pixel, b_pixel] : compressed::ranges::zip(r_chunk, g_chunk, b_chunk))
				{
					CHECK(r_pixel == static_cast<T>(255));
					CHECK(g_pixel == static_cast<T>(0));
					CHECK(b_pixel == static_cast<T>(199));
				}
			}
		}
	);
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Zip image channels larger to chunk size parametrized")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&]<typename T>([[maybe_unused]] T type)
		{
			auto channel_r_data = std::vector<T>(1024, static_cast<T>(255));
			auto channel_g_data = std::vector<T>(1024, static_cast<T>(0));
			auto channel_b_data = std::vector<T>(1024, static_cast<T>(199));

			auto image = compressed::image<T>(
				std::vector<std::vector<T>>{ channel_r_data, channel_g_data, channel_b_data },
				64,
				16,
				{},
				compressed::enums::codec::lz4,
				9,
				256, 
				768
			);

			auto [r, g, b] = image.channels(0, 1, 2);
			CHECK(r == image.channel(0));
			CHECK(g == image.channel(1));
			CHECK(b == image.channel(2));

			for (auto [r_chunk, g_chunk, b_chunk] : compressed::ranges::zip(r, g, b))
			{
				for (auto [r_pixel, g_pixel, b_pixel] : compressed::ranges::zip(r_chunk, g_chunk, b_chunk))
				{
					CHECK(r_pixel == static_cast<T>(255));
					CHECK(g_pixel == static_cast<T>(0));
					CHECK(b_pixel == static_cast<T>(199));
				}
			}
		}
	);
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Zip modify image channels parametrized")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&]<typename T>([[maybe_unused]] T type)
		{
			auto channel_r_data = std::vector<T>(128, static_cast<T>(255));
			auto channel_g_data = std::vector<T>(128, static_cast<T>(0));
			auto channel_b_data = std::vector<T>(128, static_cast<T>(199));

			auto image = compressed::image<T>(
				std::vector<std::vector<T>>{ channel_r_data, channel_g_data, channel_b_data },
				16,
				8
			);

			auto [r, g, b] = image.channels(0, 1, 2);
			CHECK(r == image.channel(0));
			CHECK(g == image.channel(1));
			CHECK(b == image.channel(2));

			for (auto [r_chunk, g_chunk, b_chunk] : compressed::ranges::zip(r, g, b))
			{
				for (auto [r_pixel, g_pixel, b_pixel] : compressed::ranges::zip(r_chunk, g_chunk, b_chunk))
				{
					r_pixel = static_cast<T>(12);
					g_pixel = static_cast<T>(13);
					b_pixel = static_cast<T>(14);
				}
			}

			for (auto [r_chunk, g_chunk, b_chunk] : compressed::ranges::zip(r, g, b))
			{
				for (auto [r_pixel, g_pixel, b_pixel] : compressed::ranges::zip(r_chunk, g_chunk, b_chunk))
				{
					CHECK(r_pixel == static_cast<T>(12));
					CHECK(g_pixel == static_cast<T>(13));
					CHECK(b_pixel == static_cast<T>(14));
				}
			}
		}
	);
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Zip modify image channels equal to chunk size parametrized")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&]<typename T>([[maybe_unused]] T type)
		{
			auto channel_r_data = std::vector<T>(1024, static_cast<T>(255));
			auto channel_g_data = std::vector<T>(1024, static_cast<T>(0));
			auto channel_b_data = std::vector<T>(1024, static_cast<T>(199));

			auto image = compressed::image<T>(
				std::vector<std::vector<T>>{ channel_r_data, channel_g_data, channel_b_data },
				64,
				16,
				{},
				compressed::enums::codec::lz4,
				9,
				256,
				1024
			);

			auto [r, g, b] = image.channels(0, 1, 2);
			CHECK(r == image.channel(0));
			CHECK(g == image.channel(1));
			CHECK(b == image.channel(2));

			for (auto [r_chunk, g_chunk, b_chunk] : compressed::ranges::zip(r, g, b))
			{
				for (auto [r_pixel, g_pixel, b_pixel] : compressed::ranges::zip(r_chunk, g_chunk, b_chunk))
				{
					r_pixel = static_cast<T>(12);
					g_pixel = static_cast<T>(13);
					b_pixel = static_cast<T>(14);
				}
			}

			for (auto [r_chunk, g_chunk, b_chunk] : compressed::ranges::zip(r, g, b))
			{
				for (auto [r_pixel, g_pixel, b_pixel] : compressed::ranges::zip(r_chunk, g_chunk, b_chunk))
				{
					CHECK(r_pixel == static_cast<T>(12));
					CHECK(g_pixel == static_cast<T>(13));
					CHECK(b_pixel == static_cast<T>(14));
				}
			}
		}
	);
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Zip modify image channels larger to chunk size parametrized")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&]<typename T>([[maybe_unused]] T type)
		{
			auto channel_r_data = std::vector<T>(1024, static_cast<T>(255));
			auto channel_g_data = std::vector<T>(1024, static_cast<T>(0));
			auto channel_b_data = std::vector<T>(1024, static_cast<T>(199));

			auto image = compressed::image<T>(
				std::vector<std::vector<T>>{ channel_r_data, channel_g_data, channel_b_data },
				64,
				16,
				{},
				compressed::enums::codec::lz4,
				9,
				256,
				768
			);

			auto [r, g, b] = image.channels(0, 1, 2);
			CHECK(r == image.channel(0));
			CHECK(g == image.channel(1));
			CHECK(b == image.channel(2));

			for (auto [r_chunk, g_chunk, b_chunk] : compressed::ranges::zip(r, g, b))
			{
				for (auto [r_pixel, g_pixel, b_pixel] : compressed::ranges::zip(r_chunk, g_chunk, b_chunk))
				{
					r_pixel = static_cast<T>(12);
					g_pixel = static_cast<T>(13);
					b_pixel = static_cast<T>(14);
				}
			}

			for (auto [r_chunk, g_chunk, b_chunk] : compressed::ranges::zip(r, g, b))
			{
				for (auto [r_pixel, g_pixel, b_pixel] : compressed::ranges::zip(r_chunk, g_chunk, b_chunk))
				{
					CHECK(r_pixel == static_cast<T>(12));
					CHECK(g_pixel == static_cast<T>(13));
					CHECK(b_pixel == static_cast<T>(14));
				}
			}
		}
	);
}