#include "doctest.h"

#include <ranges>
#include <span>
#include <vector>
#include <algorithm>
#include <thread>
#include <numeric>

#define _COMPRESSED_PROFILE 1
#include <compressed/blosc2/schunk.h>
#include <compressed/blosc2/wrapper.h>

#include "util.h"
#include "compressed/channel.h"


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Schunk: initialize with chunk size")
{
    test_util::parametrize<uint8_t, uint16_t, uint32_t, float>(
        [&]<typename T>([[maybe_unused]] T type)
        {
            compressed::detail::schunk<T> super_chunk(128, 4096);

            auto ctx = compressed::blosc2::create_decompression_context(std::thread::hardware_concurrency());

            auto compression_ctx = compressed::cpu_compression_context{
                .compression_ctx = nullptr,
                .decompression_ctx = std::move(ctx),
                .nthreads = std::thread::hardware_concurrency()
            };

            // this schunk is empty so we expect no items
            auto decompressed = super_chunk.to_uncompressed(compression_ctx);
            CHECK(decompressed.size() == 0);
        }
    );
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("Schunk: initialize with data")
{
    test_util::parametrize<uint8_t, uint16_t, uint32_t, float>(
        [&]<typename T>([[maybe_unused]] T type)
        {
            std::vector<T> data(4096);
            std::iota(data.begin(), data.end(), T{0});

            auto ctx = compressed::channel<T>::create_compression_context(
                compressed::enums::codec::lz4,
                std::thread::hardware_concurrency(),
                9,
                128,
                0
            );
            compressed::detail::schunk<T> super_chunk(std::span<const T>(data), 64, 256, std::move(ctx));

            auto decomp_ctx = compressed::channel<T>::create_compression_context(
                compressed::enums::codec::lz4,
                std::thread::hardware_concurrency(),
                9,
                128,
                0
            );
            SUBCASE("Check decompressed")
            {
                // We expect the same number of elements
                auto decompressed = super_chunk.to_uncompressed(
                    std::get<compressed::cpu_compression_context>(decomp_ctx)
                );
                CHECK(decompressed.size() == 4096);
                CHECK(decompressed == data);
            }
            SUBCASE("Get chunk")
            {
                auto chunk = super_chunk.chunk(std::get<compressed::cpu_compression_context>(decomp_ctx), size_t{0});
                CHECK(chunk.size() == 256 / sizeof(T));
            }
        }
    );
}
