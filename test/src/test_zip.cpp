#include "doctest.h"

#include <ranges>
#include <span>
#include <vector>
#include <algorithm>
#include <string>

#include <compressed/ranges.h>

#include "util.h"


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("compressed::ranges::zip sequenced loops")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&](auto type)
		{
			std::vector<decltype(type)> data_a(25, static_cast<decltype(type)>(25));
			std::vector<decltype(type)> data_b(25, static_cast<decltype(type)>(50));
			std::vector<decltype(type)> data_c(25, static_cast<decltype(type)>(75));

			auto gen = compressed::ranges::zip(data_a, data_b, data_c);
			std::for_each(std::execution::seq, gen.begin(), gen.end(), [](auto vals)
				{
					auto& [a, b, c] = vals;
					CHECK(a == static_cast<decltype(type)>(25));
					CHECK(b == static_cast<decltype(type)>(50));
					CHECK(c == static_cast<decltype(type)>(75));
				});
		});
}

// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("compressed::ranges::zip parallel loops")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&](auto type)
		{
			std::vector<decltype(type)> data_a(25, static_cast<decltype(type)>(25));
			std::vector<decltype(type)> data_b(25, static_cast<decltype(type)>(50));
			std::vector<decltype(type)> data_c(25, static_cast<decltype(type)>(75));

			auto gen = compressed::ranges::zip(data_a, data_b, data_c);
			std::for_each(std::execution::par_unseq, gen.begin(), gen.end(), [](auto vals)
				{
					auto& [a, b, c] = vals;
					CHECK(a == static_cast<decltype(type)>(25));
					CHECK(b == static_cast<decltype(type)>(50));
					CHECK(c == static_cast<decltype(type)>(75));
				});
		});
}



// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("compressed::ranges::zip regular for loop")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&](auto type)
		{
			std::vector<decltype(type)> data_a(25, static_cast<decltype(type)>(25));
			std::vector<decltype(type)> data_b(25, static_cast<decltype(type)>(50));
			std::vector<decltype(type)> data_c(25, static_cast<decltype(type)>(75));

			for (auto [a, b, c] : compressed::ranges::zip(data_a, data_b, data_c))
			{
				CHECK(a == static_cast<decltype(type)>(25));
				CHECK(b == static_cast<decltype(type)>(50));
				CHECK(c == static_cast<decltype(type)>(75));
			}
		});
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("compressed::ranges::zip serial mismatched sizes")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&](auto type)
		{
			// We expect to only iterate up until index 25 here
			std::vector<decltype(type)> data_a(30, static_cast<decltype(type)>(25));
			std::vector<decltype(type)> data_b(25, static_cast<decltype(type)>(50));
			std::vector<decltype(type)> data_c(45, static_cast<decltype(type)>(75));

			auto gen = compressed::ranges::zip(data_a, data_b, data_c);
			CHECK(gen.size() == 25);
			std::for_each(std::execution::seq, gen.begin(), gen.end(), [](auto vals)
				{
					auto& [a, b, c] = vals;
					CHECK(a == static_cast<decltype(type)>(25));
					CHECK(b == static_cast<decltype(type)>(50));
					CHECK(c == static_cast<decltype(type)>(75));
				});
		});
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("compressed::ranges::zip parallel mismatched sizes")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&](auto type)
		{
			// We expect to only iterate up until index 25 here
			std::vector<decltype(type)> data_a(30, static_cast<decltype(type)>(25));
			std::vector<decltype(type)> data_b(25, static_cast<decltype(type)>(50));
			std::vector<decltype(type)> data_c(45, static_cast<decltype(type)>(75));

			auto gen = compressed::ranges::zip(data_a, data_b, data_c);
			CHECK(gen.size() == 25);
			std::for_each(std::execution::par_unseq, gen.begin(), gen.end(), [](auto vals)
				{
					auto& [a, b, c] = vals;
					CHECK(a == static_cast<decltype(type)>(25));
					CHECK(b == static_cast<decltype(type)>(50));
					CHECK(c == static_cast<decltype(type)>(75));
				});
		});
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("compressed::ranges::zip regular for loop mismatched sizes")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&](auto type)
		{
			// We expect to only iterate up until index 25 here
			std::vector<decltype(type)> data_a(30, static_cast<decltype(type)>(25));
			std::vector<decltype(type)> data_b(25, static_cast<decltype(type)>(50));
			std::vector<decltype(type)> data_c(45, static_cast<decltype(type)>(75));

			size_t count = 0;
			for (auto [a, b, c] : compressed::ranges::zip(data_a, data_b, data_c))
			{
				CHECK(a == static_cast<decltype(type)>(25));
				CHECK(b == static_cast<decltype(type)>(50));
				CHECK(c == static_cast<decltype(type)>(75));
				++count;
			}
			CHECK(count == 25);
		});
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("compressed::ranges::zip parallel mismatched sizes modify")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&](auto type)
		{
			// We expect to only iterate up until index 25 here
			std::vector<decltype(type)> data_a(30, static_cast<decltype(type)>(25));
			std::vector<decltype(type)> data_b(25, static_cast<decltype(type)>(50));
			std::vector<decltype(type)> data_c(45, static_cast<decltype(type)>(75));

			auto gen = compressed::ranges::zip(data_a, data_b, data_c);
			CHECK(gen.size() == 25);
			std::for_each(std::execution::par_unseq, gen.begin(), gen.end(), [](auto vals)
				{
					auto& [a, b, c] = vals;
					CHECK(a == static_cast<decltype(type)>(25));
					CHECK(b == static_cast<decltype(type)>(50));
					CHECK(c == static_cast<decltype(type)>(75));

					a = 75;
					b = 49;
					c = 25;
				});

			auto gen_2 = compressed::ranges::zip(data_a, data_b, data_c);
			std::for_each(std::execution::par_unseq, gen_2.begin(), gen_2.end(), [](auto vals)
				{
					auto& [a, b, c] = vals;
					CHECK(a == static_cast<decltype(type)>(75));
					CHECK(b == static_cast<decltype(type)>(49));
					CHECK(c == static_cast<decltype(type)>(25));
				});
		});
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("compressed::ranges::zip serial mismatched sizes modify")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&](auto type)
		{
			// We expect to only iterate up until index 25 here
			std::vector<decltype(type)> data_a(30, static_cast<decltype(type)>(25));
			std::vector<decltype(type)> data_b(25, static_cast<decltype(type)>(50));
			std::vector<decltype(type)> data_c(45, static_cast<decltype(type)>(75));

			auto gen = compressed::ranges::zip(data_a, data_b, data_c);
			CHECK(gen.size() == 25);
			std::for_each(std::execution::seq, gen.begin(), gen.end(), [](auto vals)
				{
					auto& [a, b, c] = vals;
					CHECK(a == static_cast<decltype(type)>(25));
					CHECK(b == static_cast<decltype(type)>(50));
					CHECK(c == static_cast<decltype(type)>(75));

					a = 75;
					b = 49;
					c = 25;
				});

			auto gen_2 = compressed::ranges::zip(data_a, data_b, data_c);
			std::for_each(std::execution::seq, gen_2.begin(), gen_2.end(), [](auto vals)
				{
					auto& [a, b, c] = vals;
					CHECK(a == static_cast<decltype(type)>(75));
					CHECK(b == static_cast<decltype(type)>(49));
					CHECK(c == static_cast<decltype(type)>(25));
				});
		});
}


// -----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------
TEST_CASE("compressed::ranges::zip regular for loop mismatched sizes modify")
{
	test_util::parametrize<uint8_t, uint16_t, uint32_t, float>([&](auto type)
		{
			// We expect to only iterate up until index 25 here
			std::vector<decltype(type)> data_a(30, static_cast<decltype(type)>(25));
			std::vector<decltype(type)> data_b(25, static_cast<decltype(type)>(50));
			std::vector<decltype(type)> data_c(45, static_cast<decltype(type)>(75));

			size_t count = 0;
			for (auto [a, b, c] : compressed::ranges::zip(data_a, data_b, data_c))
			{
				CHECK(a == static_cast<decltype(type)>(25));
				CHECK(b == static_cast<decltype(type)>(50));
				CHECK(c == static_cast<decltype(type)>(75));

				a = static_cast<decltype(type)>(75);
				b = static_cast<decltype(type)>(49);
				c = static_cast<decltype(type)>(25);
				++count;
			}
			CHECK(count == 25);

			for (auto [a, b, c] : compressed::ranges::zip(data_a, data_b, data_c))
			{
				CHECK(a == static_cast<decltype(type)>(75));
				CHECK(b == static_cast<decltype(type)>(49));
				CHECK(c == static_cast<decltype(type)>(25));
			}

			// The zip should have only touched the first 25 elements with the rest being the same
			size_t count_2 = 0;
			for (const auto& elem : data_a)
			{
				if (count_2 < 25)
				{
					CHECK(elem == 75);
				}
				else
				{
					CHECK(elem == 25);
				}
				++count_2;
			}
		});
}