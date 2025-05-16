#include "doctest.h"

#include <ranges>
#include <span>
#include <vector>
#include <algorithm>
#include <thread>
#include <string>
#include <numeric>

#include <compressed/blosc2/schunk.h>
#include <compressed/blosc2/wrapper.h>

#include "util.h"


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Schunk: initialize with chunk size")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&]<typename T>([[maybe_unused]] T type)
		{
			compressed::blosc2::schunk<T> super_chunk(128, 4096);

			auto ctx = compressed::blosc2::create_decompression_context(std::thread::hardware_concurrency());

			// this schunk is empty so we expect no items
			auto decompressed = super_chunk.to_uncompressed(ctx);
			CHECK(decompressed.size() == 0);

			// similarly converting to schunk should work, but be empty
			auto raw_schunk = super_chunk.to_schunk();
		});
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Schunk: initialize with data")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&]<typename T>([[maybe_unused]] T type)
		{
			std::vector<T> data(4096);
			std::iota(data.begin(), data.end(), T{ 0 });

			auto ctx = compressed::blosc2::create_compression_context<T>(
				std::thread::hardware_concurrency(), 
				compressed::enums::codec::lz4, 
				9,
				128
			);
			compressed::blosc2::schunk<T> super_chunk(std::span<const T>(data), 64, 256, ctx);

			auto decomp_ctx = compressed::blosc2::create_decompression_context(std::thread::hardware_concurrency());
			SUBCASE("Check decompressed")
			{
				// We expect the same number of elements
				auto decompressed = super_chunk.to_uncompressed(decomp_ctx);
				CHECK(decompressed.size() == 4096);
				CHECK(decompressed == data);
			}
			SUBCASE("Check blosc2 schunk result")
			{
				// we also expect the right result converting to schunk
				auto raw_schunk = super_chunk.to_schunk();
				CHECK(raw_schunk->nchunks == 4096 * sizeof(T) / 256);
				CHECK(raw_schunk->nbytes / sizeof(T) == 4096);
			}
			SUBCASE("Get chunk")
			{
				auto chunk = super_chunk.chunk(decomp_ctx, 0);
				CHECK(chunk.size() == 256 / sizeof(T));
			}
		});
}