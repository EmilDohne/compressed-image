#include <vector>
#include <filesystem>
#include <execution>
#include <algorithm>
#include <span>

#include <benchmark/benchmark.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/half.h>

// enable profiling code
#define _COMPRESSED_PROFILE 1

#include <compressed/enums.h>
#include <compressed/image.h>
#include <compressed/ranges.h>
#include <compressed/detail/scoped_timer.h>

#include "memory_sampling.h"


static const std::filesystem::path s_images_path = std::filesystem::current_path() / "images";


/// Iterate all the images in ./images/ and return them to us. Our benchmarks will instantiate for each of these
std::vector<std::filesystem::path> get_images()
{
	std::vector<std::filesystem::path> result{};
	for (const auto& entry : std::filesystem::directory_iterator(s_images_path))
	{
		if (std::filesystem::is_regular_file(entry) && 
			(
				entry.path().extension() == ".exr" ||
				entry.path().extension() == ".jpg" ||
				entry.path().extension() == ".png" ||
				entry.path().extension() == ".tiff" ||
				entry.path().extension() == ".tif" ||
				entry.path().extension() == ".bmp" ||
				entry.path().extension() == ".webp" ||
				entry.path().extension() == ".tga" ||
				entry.path().extension() == ".hdr"
				)
			)
		{
			result.push_back(entry.path());
		}
	}
	return result;
}


/// Benchmark image reads using just openimageio
template <typename T>
void bench_image_read_oiio(benchmark::State& state, const std::filesystem::path& image_path)
{
	bench_util::run_with_memory_sampling(state, [&]()
		{
			auto input_ptr = OIIO::ImageInput::open(image_path);
			if (!input_ptr)
			{
				throw std::runtime_error("Failed to open image");
			}
			const OIIO::ImageSpec& spec = input_ptr->spec();
			std::vector<T> pixels(spec.width * spec.height * spec.nchannels);
			std::vector<std::vector<T>> channels;
			for (int i = 0; i < spec.nchannels; ++i)
			{
				channels.push_back(std::vector<T>(spec.width * spec.height));
			}

			auto typedesc = compressed::enums::get_type_desc<T>();
			input_ptr->read_image(0, 0, 0, spec.nchannels, typedesc, static_cast<void*>(pixels.data()));

			// We use our own deinterleaving algorithm here as that appears to be faster than using OpenImageIO's
			compressed::image_algo::deinterleave(std::span<const T>(pixels), channels);

			benchmark::ClobberMemory();
		});
}


/// Benchmark image reads using our compressed::image::read which will read the file in chunks
/// leading to less memory usage
template <typename T>
void bench_image_read_compressed(benchmark::State& state, const std::filesystem::path& image_path)
{
	double compression_ratio = 0;
	bench_util::run_with_memory_sampling(state, [&]()
		{
			auto image = compressed::image<T>::read(image_path);
			benchmark::DoNotOptimize(image);
			benchmark::ClobberMemory();
			compression_ratio = image.compression_ratio();
		});

	state.counters["Compression_Ratio"] = compression_ratio;
}


/// Benchmark iterating over the whole image normally
template <typename T>
void bench_image_iteration_normal(benchmark::State& state, const std::filesystem::path& image_path)
{
	auto input_ptr = OIIO::ImageInput::open(image_path);
	if (!input_ptr)
	{
		throw std::runtime_error("Failed to open image");
	}
	const OIIO::ImageSpec& spec = input_ptr->spec();
	std::vector<T> pixels(spec.width * spec.height * spec.nchannels);
	std::vector<std::vector<T>> channels;
	for (int i = 0; i < spec.nchannels; ++i)
	{
		channels.push_back(std::vector<T>(spec.width * spec.height));
	}

	auto typedesc = compressed::enums::get_type_desc<T>();
	input_ptr->read_image(0, 0, 0, spec.nchannels, typedesc, static_cast<void*>(pixels.data()));

	// We use our own deinterleaving algorithm here as that appears to be faster than using OpenImageIO's
	compressed::image_algo::deinterleave(std::span<const T>(pixels), channels);

	bench_util::run_with_memory_sampling(state, [&]()
		{
			for (auto& channel : channels)
			{
				std::for_each(std::execution::par_unseq, channel.begin(), channel.end(), [](auto& pixel)
					{
						pixel = static_cast<T>(25);
					});
			}
			benchmark::ClobberMemory();
		});
}


template <typename T>
void bench_image_iteration_compressed(benchmark::State& state, const std::filesystem::path& image_path)
{
	auto image = compressed::image<T>::read(image_path);
	bench_util::run_with_memory_sampling(state, [&]()
		{
			for (auto& channel : image.channels())
			{
				for (auto chunk_span : channel)
				{
					std::for_each(std::execution::par_unseq, chunk_span.begin(), chunk_span.end(), [](auto& pixel)
						{
							pixel = static_cast<T>(25);
						});
				}
			}
			benchmark::ClobberMemory();
		});
}


template <typename T>
void bench_image_iteration_compressed_zip(benchmark::State& state, const std::filesystem::path& image_path)
{
	auto image = compressed::image<T>::read(image_path);
	bench_util::run_with_memory_sampling(state, [&]()
		{
			auto [channel_r, channel_g, channel_b] = image.channels_ref(0, 1, 2);
			for (auto [chunk_r, chunk_g, chunk_b] : compressed::ranges::zip(channel_r, channel_g, channel_b))
			{
				auto gen = compressed::ranges::zip(chunk_r, chunk_g, chunk_b);
				std::for_each(std::execution::par_unseq, gen.begin(), gen.end(), [](auto pixels)
					{
						auto& [r, g, b] = pixels;
						r = static_cast<T>(25);
						g = static_cast<T>(25);
						b = static_cast<T>(25);
					});
			}
			benchmark::ClobberMemory();
		});
}



template <typename T>
void bench_image_iteration_compressed_get_decompressed(benchmark::State& state, const std::filesystem::path& image_path)
{
	auto image = compressed::image<T>::read(image_path);
	bench_util::run_with_memory_sampling(state, [&]()
		{
			auto data = image.get_decompressed();
			for (auto& channel : data)
			{
				std::for_each(std::execution::par_unseq, channel.begin(), channel.end(), [](auto& pixel)
					{
						pixel = static_cast<T>(25);
					});
			}
			benchmark::ClobberMemory();
		});
}



auto main(int argc, char** argv) -> int
{



	compressed::detail::Instrumentor::Get().BeginSession("Benchmarks");

	for (const auto& image : get_images())
	{
		// Read benchmarks
		benchmark::RegisterBenchmark(std::format("Read with OpenImageIO: {}", image.filename().string()), &bench_image_read_oiio<half>, image)->Unit(benchmark::kMillisecond)->Iterations(1);
		benchmark::RegisterBenchmark(std::format("Read with compressed::image: {}", image.filename().string()), &bench_image_read_compressed<half>, image)->Unit(benchmark::kMillisecond)->Iterations(1);

		// Iteration benchmarks
		benchmark::RegisterBenchmark(std::format("Modify image normal: {}", image.filename().string()), &bench_image_iteration_normal<half>, image)->Unit(benchmark::kMillisecond)->Iterations(1);
		benchmark::RegisterBenchmark(std::format("Modify image compressed: {}", image.filename().string()), &bench_image_iteration_compressed<half>, image)->Unit(benchmark::kMillisecond)->Iterations(1);
		benchmark::RegisterBenchmark(std::format("Modify image compressed zip rgb: {}", image.filename().string()), &bench_image_iteration_compressed_zip<half>, image)->Unit(benchmark::kMillisecond)->Iterations(1);
		benchmark::RegisterBenchmark(std::format("Modify image get decompressed: {}", image.filename().string()), &bench_image_iteration_compressed_get_decompressed<half>, image)->Unit(benchmark::kMillisecond)->Iterations(1);
	}

	benchmark::Initialize(&argc, argv);
	benchmark::RunSpecifiedBenchmarks();

	compressed::detail::Instrumentor::Get().EndSession();
}