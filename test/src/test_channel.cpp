#include "doctest.h"

#include <ranges>
#include <span>
#include <vector>
#include <algorithm>
#include <string>

#include <OpenImageIO/half.h>

#include <compressed/channel.h>
#include <compressed/blosc2/wrapper.h>

#include "util.h"



TEST_CASE("Initialize channel from incorrect schunk"
	* doctest::no_breaks(true)
	* doctest::no_output(true)
	* doctest::should_fail(true)
)
{
	auto schunk = compressed::blosc2::schunk<uint8_t>();
	auto channel = compressed::channel<uint8_t>(std::move(schunk), 1, 1);
}


TEST_CASE("Initialize channel from incorrect span"
	* doctest::no_breaks(true)
	* doctest::no_output(true)
	* doctest::should_fail(true)
)
{
	auto vec = std::vector<uint8_t>(50);
	auto channel = compressed::channel<uint8_t>(std::span<uint8_t>(vec), 1, 1);
}


TEST_CASE("Roundtrip channel creation")
{
	auto vec = std::vector<uint8_t>(50);
	std::iota(vec.begin(), vec.end(), 0);

	auto channel = compressed::channel<uint8_t>(std::span<uint8_t>(vec), 10, 5);
	auto roundtripped = channel.get_decompressed();

	CHECK(vec == roundtripped);
}


TEST_CASE("Roundtrip channel creation larger than chunksize")
{
	auto vec = std::vector<uint8_t>(8192);
	std::iota(vec.begin(), vec.end(), 0);

	auto channel = compressed::channel<uint8_t>(std::span<uint8_t>(vec), 128, 64, compressed::enums::codec::lz4, 9, 128, 4096);
	auto roundtripped = channel.get_decompressed();

	CHECK(vec == roundtripped);
}


TEST_CASE("Channel get attributes"
)
{
	auto vec = std::vector<uint8_t>(50);
	auto channel = compressed::channel<uint8_t>(std::span<uint8_t>(vec), 10, 5, compressed::enums::codec::blosclz, 9);

	CHECK(channel.width() == 10);
	CHECK(channel.height() == 5);
	CHECK(channel.compression() == compressed::enums::codec::blosclz);
	CHECK(channel.compression_context() != nullptr);
	CHECK(channel.decompression_context() != nullptr);
	CHECK(channel.uncompressed_size() == 50);
	CHECK(channel.num_chunks() == 1);
}


TEST_CASE("Channel iterate")
{
	auto vec = std::vector<uint16_t>(128, 255);
	auto channel = compressed::channel<uint16_t>(std::span<uint16_t>(vec), 16, 8);

	SUBCASE("Read")
	{
		for (auto chunk_span : channel)
		{
			for (auto& pixel : chunk_span)
			{
				CHECK(pixel == 255);
			}
		}
	}

	SUBCASE("Modify")
	{
		for (auto chunk_span : channel)
		{
			for (auto& pixel : chunk_span)
			{
				pixel = 128;
			}
		}

		for (auto chunk_span : channel)
		{
			for (auto& pixel : chunk_span)
			{
				CHECK(pixel == 128);
			}
		}
	}
}