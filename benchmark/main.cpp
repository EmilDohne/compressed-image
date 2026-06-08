#include <vector>
#include <filesystem>
#include <execution>
#include <algorithm>
#include <span>
#include <string_view>
#include <utility>

#include <benchmark/benchmark.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/half.h>

// Re-enable this if you wish to see json profiling of all the individual function calls. This adds some overhead so
// we disable this by default. The json is called 'results.json' and will live in the same directory as the benchmark
// executable. The trace is ui.perfetto.dev compatible.
// Note:
//      This more heavily affects gpu benchmarks as we do more verbose tracking there. This should only be used if you
//      want to debug performance issues in the library as it might skew the results.
// #define _COMPRESSED_PROFILE 1

#include <compressed/enums.h>
#include <compressed/image.h>
#include <compressed/ranges.h>
#include <compressed/detail/scoped_timer.h>

#include "util/memory_sampling.h"
#include "util.h"

/// The number of executions per benchmark
constexpr static size_t s_iterations = 3;

// ============================================================================
// BENCHMARK DEFINITIONS
// ============================================================================

template <typename T>
void bench_image_read_oiio(benchmark::State& state, const std::filesystem::path& image_path)
{
    bench_util::run_with_memory_sampling(
        state,
        [&]()
        {
            _COMPRESSED_PROFILE_FUNCTION();
            auto input_ptr = OIIO::ImageInput::open(image_path);
            if (!input_ptr) return;

            const OIIO::ImageSpec& spec = input_ptr->spec();
            std::vector<T> pixels(static_cast<size_t>(spec.width) * spec.height * spec.nchannels);
            std::vector<std::vector<T>> channels(
                spec.nchannels,
                std::vector<T>(static_cast<size_t>(spec.width) * spec.height)
            );

            auto typedesc = compressed::enums::get_type_desc<T>();
            input_ptr->read_image(0, 0, 0, spec.nchannels, typedesc, static_cast<void*>(pixels.data()));

            compressed::image_algo::deinterleave(std::span<const T>(pixels), channels);
            benchmark::ClobberMemory();
        }
    );
}

template <typename T, compressed::enums::codec Codec>
void bench_image_read_compressed(benchmark::State& state, const std::filesystem::path& image_path)
{
    size_t compressed_size = 0;
    size_t uncompressed_size = 0;
    double compression_ratio = 0;

    bench_util::run_with_memory_sampling(
        state,
        [&]()
        {
            _COMPRESSED_PROFILE_FUNCTION();
            // Pass 0 for subimage, followed by the specific Codec
            auto image = compressed::image<T>::read(image_path, 0, Codec);
            benchmark::DoNotOptimize(image);
            benchmark::ClobberMemory();
            compressed_size = image.compressed_bytes();
            uncompressed_size = image.uncompressed_bytes();
            compression_ratio = image.compression_ratio();
        }
    );

    state.counters["compressed_bytes"] = static_cast<double>(compressed_size);
    state.counters["uncompressed_bytes"] = static_cast<double>(uncompressed_size);
    state.counters["compression_ratio"] = compression_ratio;
}

template <typename T>
void bench_image_iteration_normal(benchmark::State& state, const std::filesystem::path& image_path)
{
    auto input_ptr = OIIO::ImageInput::open(image_path);
    if (!input_ptr) return;
    const OIIO::ImageSpec& spec = input_ptr->spec();
    std::vector<T> pixels(spec.width * spec.height * spec.nchannels);
    std::vector<std::vector<T>> channels(spec.nchannels, std::vector<T>(spec.width * spec.height));


    bench_util::run_with_memory_sampling(
        state,
        [&]()
        {
            _COMPRESSED_PROFILE_FUNCTION();

            auto typedesc = compressed::enums::get_type_desc<T>();
            input_ptr->read_image(0, 0, 0, spec.nchannels, typedesc, static_cast<void*>(pixels.data()));
            compressed::image_algo::deinterleave(std::span<const T>(pixels), channels);


            for (auto& channel : channels)
            {
                std::for_each(
                    std::execution::par_unseq,
                    channel.begin(),
                    channel.end(),
                    [](auto& pixel)
                    {
                        pixel = static_cast<T>(25);
                    }
                );
            }
            benchmark::ClobberMemory();
        }
    );
}

template <typename T, compressed::enums::codec Codec>
void bench_image_iteration_compressed(benchmark::State& state, const std::filesystem::path& image_path)
{
    auto image = compressed::image<T>::read(image_path, 0, Codec);

    bench_util::run_with_memory_sampling(
        state,
        [&]()
        {
            _COMPRESSED_PROFILE_FUNCTION();

            for (auto& channel : image.channels())
            {
                for (auto chunk_span : channel)
                {
                    std::for_each(
                        std::execution::par_unseq,
                        chunk_span.begin(),
                        chunk_span.end(),
                        [](auto& pixel)
                        {
                            pixel = static_cast<T>(25);
                        }
                    );
                }
            }
            benchmark::ClobberMemory();
        }
    );
}

template <typename T, compressed::enums::codec Codec>
void bench_image_iteration_compressed_zip(benchmark::State& state, const std::filesystem::path& image_path)
{
    auto image = compressed::image<T>::read(image_path, 0, Codec);


    bench_util::run_with_memory_sampling(
        state,
        [&]()
        {
            _COMPRESSED_PROFILE_FUNCTION();


            auto [channel_r, channel_g, channel_b] = image.channels(0, 1, 2);
            for (auto [chunk_r, chunk_g, chunk_b] : compressed::ranges::zip(channel_r, channel_g, channel_b))
            {
                auto gen = compressed::ranges::zip(chunk_r, chunk_g, chunk_b);
                std::for_each(
                    std::execution::par_unseq,
                    gen.begin(),
                    gen.end(),
                    [](auto pixels)
                    {
                        auto& [r, g, b] = pixels;
                        r = static_cast<T>(25);
                        g = static_cast<T>(25);
                        b = static_cast<T>(25);
                    }
                );
            }
            benchmark::ClobberMemory();
        }
    );
}

template <typename T, compressed::enums::codec Codec>
void bench_image_iteration_compressed_get_decompressed(benchmark::State& state, const std::filesystem::path& image_path)
{
    auto image = compressed::image<T>::read(image_path, 0, Codec);


    bench_util::run_with_memory_sampling(
        state,
        [&]()
        {
            _COMPRESSED_PROFILE_FUNCTION();

            auto data = image.get_decompressed();
            for (auto& channel : data)
            {
                std::for_each(
                    std::execution::par_unseq,
                    channel.begin(),
                    channel.end(),
                    [](auto& pixel)
                    {
                        pixel = static_cast<T>(25);
                    }
                );
            }
            benchmark::ClobberMemory();
        }
    );
}

// ============================================================================
// PARAMETRIZATION & REGISTRATION
// ============================================================================

constexpr std::array s_all_codecs = {
    compressed::enums::codec::blosclz,
    compressed::enums::codec::lz4,
    compressed::enums::codec::lz4hc,
    compressed::enums::codec::zstd,
    compressed::enums::codec::lz4_gpu,
    compressed::enums::codec::snappy_gpu,
    compressed::enums::codec::zstd_gpu,
    compressed::enums::codec::deflate_gpu,
    compressed::enums::codec::gdeflate_gpu,
    compressed::enums::codec::cascaded_gpu
};

/// Helper template to register a single codec configuration
template <typename T, size_t CodecIdx>
void register_codec_benchmarks_for_type(const std::filesystem::path& image,
                                        const std::string& filename,
                                        std::string_view tname)
{
    constexpr auto codec = s_all_codecs[CodecIdx];
    const std::string_view cname = compressed::enums::to_string(codec);

    benchmark::RegisterBenchmark(
            std::format("read_compressed<{}_{}>/{}", tname, cname, filename),
            &bench_image_read_compressed<T, codec>,
            image
        )
        ->Unit(benchmark::kMillisecond)->Iterations(s_iterations);

    benchmark::RegisterBenchmark(
            std::format("iter_chunked<{}_{}>/{}", tname, cname, filename),
            &bench_image_iteration_compressed<T, codec>,
            image
        )
        ->Unit(benchmark::kMillisecond)->Iterations(s_iterations);

    benchmark::RegisterBenchmark(
            std::format("iter_zip_rgb<{}_{}>/{}", tname, cname, filename),
            &bench_image_iteration_compressed_zip<T, codec>,
            image
        )
        ->Unit(benchmark::kMillisecond)->Iterations(s_iterations);
    benchmark::RegisterBenchmark(
            std::format("iter_get_decompressed<{}_{}>/{}", tname, cname, filename),
            &bench_image_iteration_compressed_get_decompressed<T, codec>,
            image
        )
        ->Unit(benchmark::kMillisecond)->Iterations(s_iterations);
}

/// Unrolls the global codec array via index_sequence mapping
template <typename T, size_t... Is>
void register_benchmarks_for_type_impl(const std::filesystem::path& image, std::index_sequence<Is...>)
{
    const std::string filename = image.filename().string();
    const std::string_view tname = type_name<T>();

    // 1. Core Framework Overheads (Independent of Codecs - registered once per type)
    benchmark::RegisterBenchmark(
            std::format("read_oiio<{}>/{}", tname, filename),
            &bench_image_read_oiio<T>,
            image
        )
        ->Unit(benchmark::kMillisecond)->Iterations(s_iterations);

    benchmark::RegisterBenchmark(
            std::format("iter_no_compression<{}>/{}", tname, filename),
            &bench_image_iteration_normal<T>,
            image
        )
        ->Unit(benchmark::kMillisecond)->Iterations(s_iterations);

    // Codec Dependent Framework Benchmarks (Fold-expanded across all indices)
    (register_codec_benchmarks_for_type<T, Is>(image, filename, tname), ...);
}

/// Master registration function parameterized over types and codecs
template <typename T>
void register_benchmarks_for_type(const std::filesystem::path& image)
{
    register_benchmarks_for_type_impl<T>(image, std::make_index_sequence<s_all_codecs.size()>());
}

/// Variadic template manager to expand types group
template <typename... Types>
void register_all_permutations(const std::vector<std::filesystem::path>& images)
{
    for (const auto& image : images)
    {
        // Fold expression expanding the target types sequentially
        (register_benchmarks_for_type<Types>(image), ...);
    }
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

auto main(int argc, char** argv) -> int
{
    compressed::detail::Instrumentor::Get().BeginSession("Benchmarks");

    const auto images = get_images();

    register_all_permutations<uint8_t, uint16_t, uint32_t, Imath::half, float>(images);

    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();

    compressed::detail::Instrumentor::Get().EndSession();
    return 0;
}
