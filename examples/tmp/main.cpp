#include <execution>
#include <algorithm>
#include <iostream>
#include <chrono>

#include <compressed/image.h>
#include <OpenImageIO/half.h>
#include <OpenImageIO/imageio.h>


constexpr int WARMUP_RUNS = 3;
constexpr int TIMED_RUNS = 5;


template <typename Func>
void benchmark(Func func, const std::string& label) 
{
	// Warm-up phase (not timed)
	for (auto _ : std::views::iota(0, WARMUP_RUNS)) 
	{
		func();
	}

	// Timed runs
	std::vector<double> timings;
	for (auto _ : std::views::iota(0, TIMED_RUNS))
	{
		auto start = std::chrono::high_resolution_clock::now();
		func();
		auto end = std::chrono::high_resolution_clock::now();

		std::chrono::duration<double> duration = end - start;
		timings.push_back(duration.count());
	}

	// Compute average time
	double total_time = 0.0;
	for (double t : timings) 
	{
		total_time += t;
	}
	double avg_time = total_time / TIMED_RUNS;

	std::cout << label << " average time over " << TIMED_RUNS << " runs: "
		<< avg_time << " seconds\n";
}


std::vector<std::vector<half>> read_oiio(std::string filepath)
{
	auto in = OIIO::ImageInput::open(filepath);
	if (!in) 
	{
		throw std::runtime_error("Failed to open image");
	}
	const OIIO::ImageSpec& spec = in->spec();
	std::vector<half> pixels(spec.width * spec.height * spec.nchannels);
	std::vector<std::vector<half>> channels;
	for (size_t i = 0; i < spec.nchannels; ++i)
	{
		channels.push_back(std::vector<half>(spec.width * spec.height));
	}
	in->read_image(0, 0, 0, spec.nchannels, OIIO::TypeDesc::HALF, static_cast<void*>(pixels.data()));
	compressed::image_algo::deinterleave(std::span<const half>(pixels), channels);
	return channels;
}

void check_correctness(const std::string filepath)
{

	auto image_oiio = read_oiio(filepath);
	auto image_compressed = compressed::image<half>::read(filepath);

	{
		auto data_image = image_compressed.get_decompressed();
		for (auto channel_idx : std::views::iota(static_cast<size_t>(0), image_oiio.size()))
		{
			if (image_oiio[channel_idx].size() != data_image[channel_idx].size())
			{
				throw std::runtime_error("Size mismatch");
			}
			for (auto image_idx : std::views::iota(static_cast<size_t>(0), image_oiio[channel_idx].size()))
			{
				if (image_oiio[channel_idx][image_idx] != data_image[channel_idx][image_idx])
				{
					throw std::runtime_error("Item mismatch");
				}
			}
		}
	}
}


auto main() -> int
{
	//const std::string filepath = "C:/Users/emild/Desktop/HarvestImages/masterLayer1.exr";
	//const std::string filepath = "C:/Users/emild/Desktop/Camera_FULL_FRONT_HDR_Night.exr";
	const std::string filepath = "C:/Users/emild/Desktop/Opel_Mokka_Car_Basis_v01.0000.exr";

	//check_correctness(filepath);

	/// Benchmark regular OIIO reads
	/*benchmark([&]() {
		auto in = OIIO::ImageInput::open(filepath);
		if (!in) 
		{
			std::cerr << "Failed to open image: " << filepath << "\n";
			return;
		}
		const OIIO::ImageSpec& spec = in->spec();
		std::vector<half> pixels(static_cast<size_t>(spec.width) * spec.height * spec.nchannels);
		std::vector<std::vector<half>> channels;
		for (size_t i = 0; i < spec.nchannels; ++i)
		{
			channels.push_back(std::vector<half>(static_cast<size_t>(spec.width) * spec.height));
		}
		in->read_image(0, 0, 0, spec.nchannels, OIIO::TypeDesc::HALF, static_cast<void*>(pixels.data()));
		compressed::image_algo::deinterleave(std::span<const half>(pixels), channels);

		}, "read_oiio");

	benchmark([&]() {
		auto image = compressed::image<half>::read(filepath, compressed::enums::codec::lz4, 5);
		}, "compressed::image::read()");*/

	{
		auto image = compressed::image<half>::read(filepath, compressed::enums::codec::lz4, 9);
		image.print_statistics();


		benchmark([&]() {
			auto& r = image.channel_ref("R");
			for (auto chunk : r)
			{
				std::for_each(std::execution::par_unseq, chunk.begin(), chunk.end(), [](half& value)
					{
						value += .1f;
					});
			}
			}, "compressed::image modify");
	}

	{
		auto in = OIIO::ImageInput::open(filepath);
		if (!in)
		{
			throw std::runtime_error("");
		}
		const OIIO::ImageSpec& spec = in->spec();
		std::vector<half> pixels(static_cast<size_t>(spec.width) * spec.height * spec.nchannels);
		std::vector<std::vector<half>> channels;
		for (size_t i = 0; i < spec.nchannels; ++i)
		{
			channels.push_back(std::vector<half>(static_cast<size_t>(spec.width) * spec.height));
		}
		in->read_image(0, 0, 0, spec.nchannels, OIIO::TypeDesc::HALF, static_cast<void*>(pixels.data()));
		compressed::image_algo::deinterleave(std::span<const half>(pixels), channels);

		benchmark([&]() {
			std::for_each(std::execution::par_unseq, channels[0].begin(), channels[0].end(), [](half& value)
				{
					value += .1f;
				});
			}, "oiio image modify");
	}
}