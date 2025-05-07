#include "doctest.h"

#include <ranges>
#include <span>
#include <vector>
#include <algorithm>
#include <string>

#include <compressed/image.h>
#include <compressed/iterators/iterator.h>

#include "util.h"


TEST_CASE("Iterator: serial access")
{
	std::string name = "uv_grid_2048x2048.jpg";
	auto path = std::filesystem::current_path() / "images" / name;
	auto image = compressed::image<uint8_t>::read(path);

	auto& r = image.channel_ref(0);
	size_t count = 0;
	for (const auto& chunk : r)
	{
		CHECK(chunk.chunk_index() == count);
		++count;
	}
}


TEST_CASE("Iterator: iterate out of bounds"
	* doctest::no_breaks(true)
	* doctest::no_output(true)
	* doctest::should_fail(true))
{
	std::string name = "uv_grid_2048x2048.jpg";
	auto path = std::filesystem::current_path() / "images" / name;
	auto image = compressed::image<uint8_t>::read(path);

	auto& r = image.channel_ref(0);
	auto it = r.begin();
	++it;
	++it;
}



TEST_CASE("Iterator: comparison")
{
	std::string name = "uv_grid_2048x2048.jpg";
	auto path = std::filesystem::current_path() / "images" / name;
	auto image = compressed::image<uint8_t>::read(
		path,
		compressed::enums::codec::lz4,
		9,
		4096, 
		16384
	);

	auto& r = image.channel_ref(0);
	auto it = r.begin();
	auto it_2 = r.begin();

	CHECK(it == it_2);
	++it;
	CHECK(it != it_2);

	// Different image, iterator should not match
	auto image_2 = compressed::image<uint8_t>::read(
		path,
		compressed::enums::codec::lz4,
		9,
		4096,
		16384
	);
	auto& r_2 = image_2.channel_ref(0);
	auto it_other = r_2.begin();

	CHECK(it_other != it);
	CHECK(it_other != it_2);
}
