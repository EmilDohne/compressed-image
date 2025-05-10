#include "doctest.h"

#include <ranges>
#include <span>
#include <vector>
#include <algorithm>
#include <string>

#include <compressed/containers/chunk_span.h>

#include "util.h"


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Get coordinates in base-chunk")
{
	std::vector<uint8_t> data(50);
	auto span_container = compressed::container::chunk_span<uint8_t>(std::span<uint8_t>(data), 10, 5, 0, compressed::s_default_chunksize);

	CHECK(span_container.x(9) == 9);
	CHECK(span_container.y(5) == 0);

	CHECK(span_container.x(15) == 5);
	CHECK(span_container.y(15) == 1);
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Get coordinates in non-base chunk")
{
	std::vector<uint8_t> data(50);
	auto span_container = compressed::container::chunk_span<uint8_t>(std::span<uint8_t>(data), 128, 128, 1, 128);

	CHECK(span_container.x(9) == 9);
	CHECK(span_container.y(5) == 1);
	CHECK(span_container.x(135) == 7);
	CHECK(span_container.y(129) == 2);
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Iter over chunk")
{
	std::vector<uint8_t> data(50, 5);
	auto span_container = compressed::container::chunk_span<uint8_t>(std::span<uint8_t>(data), 50, 1, 0, compressed::s_default_chunksize);

	size_t count = 0;
	for (const auto& pixel : span_container)
	{
		CHECK(pixel == 5);
		++count;
	}
	CHECK(count == 50);
}
