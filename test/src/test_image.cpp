#include "doctest.h"

#include <ranges>
#include <string>
#include <cstdint>

#include <OpenImageIO/half.h>

#include <compressed/image.h>
#include <compressed/ranges.h>
#include <compressed/util.h>
#include <compressed/blosc2_wrapper.h>

#include "util.h"


TEST_CASE("Read compressed file smaller than one chunk")
{
	std::string name = "uv_grid_2048x2048.jpg";
	auto path = std::filesystem::current_path() / "images" / name;
	std::cout << path << std::endl;

	auto image = compressed::image<uint8_t, compressed::s_default_blocksize, compressed::s_default_chunksize * 2>::read(path);
	auto image_data = image.get_decompressed();
	auto image_ref = test_util::read_oiio<uint8_t>(path);


	test_util::compare_images(image_data, image_ref, name);
}


TEST_CASE("Read compressed file exactly than one chunk")
{
	std::string name = "uv_grid_2048x2048.jpg";
	auto path = std::filesystem::current_path() / "images" / name;

	auto image = compressed::image<uint8_t>::read(path);
	auto image_data = image.get_decompressed();
	auto image_ref = test_util::read_oiio<uint8_t>(path);


	test_util::compare_images(image_data, image_ref, name);
}


TEST_CASE("Read compressed file larger than one chunk")
{
	std::string name = "multilayer_2560x1440.exr";
	auto path = std::filesystem::current_path() / "images" / name;

	auto image = compressed::image<float, compressed::s_default_blocksize, compressed::s_default_chunksize / 2>::read(path);
	auto image_data = image.get_decompressed();
	auto image_ref = test_util::read_oiio<float>(path);


	test_util::compare_images(image_data, image_ref, name);
}


TEST_CASE("Initialize image and iterate parametrized")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&](auto type)
		{
			auto channel_r_data = std::vector<decltype(type)>(128, static_cast<decltype(type)>(255));

			auto image = compressed::image<decltype(type)>(
				std::vector<std::vector<decltype(type)>>{ channel_r_data},
				16,
				8
			);

			SUBCASE("Read")
			{
				auto& r_ref = image.channel_ref(0);
				for (auto chunk : r_ref)
				{
					for (auto& pixel : chunk)
					{
						CHECK(pixel == static_cast<decltype(type)>(255));
					}
				}
			}

			SUBCASE("Modify")
			{
				auto& r_ref = image.channel_ref(0);
				for (auto chunk : r_ref)
				{
					for (auto& pixel : chunk)
					{
						pixel = static_cast<decltype(type)>(128);
					}
				}

				auto& r_ref_2 = image.channel_ref(0);
				for (auto chunk_ : r_ref_2)
				{
					for (auto& pixel : chunk_)
					{
						CHECK(pixel == static_cast<decltype(type)>(128));
					}
				}
			}
		}
	);

}


TEST_CASE("Zip image channels parametrized")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&](auto type)
		{
			auto channel_r_data = std::vector<decltype(type)>(128, static_cast<decltype(type)>(255));
			auto channel_g_data = std::vector<decltype(type)>(128, static_cast<decltype(type)>(0));
			auto channel_b_data = std::vector<decltype(type)>(128, static_cast<decltype(type)>(199));

			auto image = compressed::image<decltype(type)>(
				std::vector<std::vector<decltype(type)>>{ channel_r_data, channel_g_data, channel_b_data },
				16,
				8
			);

			auto [r, g, b] = image.channels_ref(0, 1, 2);
			CHECK(r == image.channel_ref(0));
			CHECK(g == image.channel_ref(1));
			CHECK(b == image.channel_ref(2));

			for (auto [r_chunk, g_chunk, b_chunk] : compressed::ranges::zip(r, g, b))
			{
				for (auto [r_pixel, g_pixel, b_pixel] : compressed::ranges::zip(r_chunk, g_chunk, b_chunk))
				{
					CHECK(r_pixel == static_cast<decltype(type)>(255));
					CHECK(g_pixel == static_cast<decltype(type)>(0));
					CHECK(b_pixel == static_cast<decltype(type)>(199));
				}
			}
		}
	);
}


TEST_CASE("Zip image channels equal to chunk size parametrized")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&](auto type)
		{
			auto channel_r_data = std::vector<decltype(type)>(1024, static_cast<decltype(type)>(255));
			auto channel_g_data = std::vector<decltype(type)>(1024, static_cast<decltype(type)>(0));
			auto channel_b_data = std::vector<decltype(type)>(1024, static_cast<decltype(type)>(199));

			auto image = compressed::image<decltype(type), 256, 1024>(
				std::vector<std::vector<decltype(type)>>{ channel_r_data, channel_g_data, channel_b_data },
				64,
				16
			);

			auto [r, g, b] = image.channels_ref(0, 1, 2);
			CHECK(r == image.channel_ref(0));
			CHECK(g == image.channel_ref(1));
			CHECK(b == image.channel_ref(2));

			for (auto [r_chunk, g_chunk, b_chunk] : compressed::ranges::zip(r, g, b))
			{
				for (auto [r_pixel, g_pixel, b_pixel] : compressed::ranges::zip(r_chunk, g_chunk, b_chunk))
				{
					CHECK(r_pixel == static_cast<decltype(type)>(255));
					CHECK(g_pixel == static_cast<decltype(type)>(0));
					CHECK(b_pixel == static_cast<decltype(type)>(199));
				}
			}
		}
	);
}


TEST_CASE("Zip image channels larger to chunk size parametrized")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&](auto type)
		{
			auto channel_r_data = std::vector<decltype(type)>(1024, static_cast<decltype(type)>(255));
			auto channel_g_data = std::vector<decltype(type)>(1024, static_cast<decltype(type)>(0));
			auto channel_b_data = std::vector<decltype(type)>(1024, static_cast<decltype(type)>(199));

			auto image = compressed::image<decltype(type), 256, 768>(
				std::vector<std::vector<decltype(type)>>{ channel_r_data, channel_g_data, channel_b_data },
				64,
				16
			);

			auto [r, g, b] = image.channels_ref(0, 1, 2);
			CHECK(r == image.channel_ref(0));
			CHECK(g == image.channel_ref(1));
			CHECK(b == image.channel_ref(2));

			for (auto [r_chunk, g_chunk, b_chunk] : compressed::ranges::zip(r, g, b))
			{
				for (auto [r_pixel, g_pixel, b_pixel] : compressed::ranges::zip(r_chunk, g_chunk, b_chunk))
				{
					CHECK(r_pixel == static_cast<decltype(type)>(255));
					CHECK(g_pixel == static_cast<decltype(type)>(0));
					CHECK(b_pixel == static_cast<decltype(type)>(199));
				}
			}
		}
	);
}


TEST_CASE("Zip modify image channels parametrized")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&](auto type)
		{
			auto channel_r_data = std::vector<decltype(type)>(128, static_cast<decltype(type)>(255));
			auto channel_g_data = std::vector<decltype(type)>(128, static_cast<decltype(type)>(0));
			auto channel_b_data = std::vector<decltype(type)>(128, static_cast<decltype(type)>(199));

			auto image = compressed::image<decltype(type)>(
				std::vector<std::vector<decltype(type)>>{ channel_r_data, channel_g_data, channel_b_data },
				16,
				8
			);

			auto [r, g, b] = image.channels_ref(0, 1, 2);
			CHECK(r == image.channel_ref(0));
			CHECK(g == image.channel_ref(1));
			CHECK(b == image.channel_ref(2));

			for (auto [r_chunk, g_chunk, b_chunk] : compressed::ranges::zip(r, g, b))
			{
				for (auto [r_pixel, g_pixel, b_pixel] : compressed::ranges::zip(r_chunk, g_chunk, b_chunk))
				{
					r_pixel = static_cast<decltype(type)>(12);
					g_pixel = static_cast<decltype(type)>(13);
					b_pixel = static_cast<decltype(type)>(14);
				}
			}

			for (auto [r_chunk, g_chunk, b_chunk] : compressed::ranges::zip(r, g, b))
			{
				for (auto [r_pixel, g_pixel, b_pixel] : compressed::ranges::zip(r_chunk, g_chunk, b_chunk))
				{
					CHECK(r_pixel == static_cast<decltype(type)>(12));
					CHECK(g_pixel == static_cast<decltype(type)>(13));
					CHECK(b_pixel == static_cast<decltype(type)>(14));
				}
			}
		}
	);
}


TEST_CASE("Zip modify image channels equal to chunk size parametrized")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&](auto type)
		{
			auto channel_r_data = std::vector<decltype(type)>(1024, static_cast<decltype(type)>(255));
			auto channel_g_data = std::vector<decltype(type)>(1024, static_cast<decltype(type)>(0));
			auto channel_b_data = std::vector<decltype(type)>(1024, static_cast<decltype(type)>(199));

			auto image = compressed::image<decltype(type), 256, 1024>(
				std::vector<std::vector<decltype(type)>>{ channel_r_data, channel_g_data, channel_b_data },
				64,
				16
			);

			auto [r, g, b] = image.channels_ref(0, 1, 2);
			CHECK(r == image.channel_ref(0));
			CHECK(g == image.channel_ref(1));
			CHECK(b == image.channel_ref(2));

			for (auto [r_chunk, g_chunk, b_chunk] : compressed::ranges::zip(r, g, b))
			{
				for (auto [r_pixel, g_pixel, b_pixel] : compressed::ranges::zip(r_chunk, g_chunk, b_chunk))
				{
					r_pixel = static_cast<decltype(type)>(12);
					g_pixel = static_cast<decltype(type)>(13);
					b_pixel = static_cast<decltype(type)>(14);
				}
			}

			for (auto [r_chunk, g_chunk, b_chunk] : compressed::ranges::zip(r, g, b))
			{
				for (auto [r_pixel, g_pixel, b_pixel] : compressed::ranges::zip(r_chunk, g_chunk, b_chunk))
				{
					CHECK(r_pixel == static_cast<decltype(type)>(12));
					CHECK(g_pixel == static_cast<decltype(type)>(13));
					CHECK(b_pixel == static_cast<decltype(type)>(14));
				}
			}
		}
	);
}


TEST_CASE("Zip modify image channels larger to chunk size parametrized")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&](auto type)
		{
			auto channel_r_data = std::vector<decltype(type)>(1024, static_cast<decltype(type)>(255));
			auto channel_g_data = std::vector<decltype(type)>(1024, static_cast<decltype(type)>(0));
			auto channel_b_data = std::vector<decltype(type)>(1024, static_cast<decltype(type)>(199));

			auto image = compressed::image<decltype(type), 256, 768>(
				std::vector<std::vector<decltype(type)>>{ channel_r_data, channel_g_data, channel_b_data },
				64,
				16
			);

			auto [r, g, b] = image.channels_ref(0, 1, 2);
			CHECK(r == image.channel_ref(0));
			CHECK(g == image.channel_ref(1));
			CHECK(b == image.channel_ref(2));

			for (auto [r_chunk, g_chunk, b_chunk] : compressed::ranges::zip(r, g, b))
			{
				for (auto [r_pixel, g_pixel, b_pixel] : compressed::ranges::zip(r_chunk, g_chunk, b_chunk))
				{
					r_pixel = static_cast<decltype(type)>(12);
					g_pixel = static_cast<decltype(type)>(13);
					b_pixel = static_cast<decltype(type)>(14);
				}
			}

			for (auto [r_chunk, g_chunk, b_chunk] : compressed::ranges::zip(r, g, b))
			{
				for (auto [r_pixel, g_pixel, b_pixel] : compressed::ranges::zip(r_chunk, g_chunk, b_chunk))
				{
					CHECK(r_pixel == static_cast<decltype(type)>(12));
					CHECK(g_pixel == static_cast<decltype(type)>(13));
					CHECK(b_pixel == static_cast<decltype(type)>(14));
				}
			}
		}
	);
}