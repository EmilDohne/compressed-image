#pragma once

#include <execution>
#include <ranges>
#include <vector>
#include <span>
#include <filesystem>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/half.h>

#include <compressed/image_algo.h>
#include <compressed/enums.h>


namespace test_util
{

	/// Read the image using OpenImageIO (OIIO) and deinterleave all the channels into discrete buffers.
	/// 
	/// This function opens an image file using OIIO, reads its pixel data into a single buffer, 
	/// and then separates the interleaved channel data into individual channel buffers.
	/// 
	/// \tparam T The pixel data type (e.g., uint8_t, float).
	/// \param filepath The file path to the image.
	/// \return A vector of vectors, where each inner vector represents a deinterleaved channel.
	/// \throws std::runtime_error if the image fails to open or read.
	template <typename T>
	std::vector<std::vector<T>> read_oiio(std::filesystem::path filepath, int subimage = 0)
	{
		auto input_ptr = OIIO::ImageInput::open(filepath.string());
		if (!input_ptr)
		{
			throw std::runtime_error(std::format("Failed to open image {}", filepath.string()));
		}
		auto res = input_ptr->seek_subimage(subimage, 0);
		if (!res)
		{
			throw std::runtime_error(std::format("Image {} does not contain subimage {}", filepath.string(), subimage));
		}
		const OIIO::ImageSpec& spec = input_ptr->spec();
		std::vector<T> pixels(static_cast<size_t>(spec.width) * spec.height * spec.nchannels);
		std::vector<std::vector<T>> channels;
		for ([[maybe_unused]] auto _ : std::views::iota(0, spec.nchannels))
		{
			channels.push_back(std::vector<T>(static_cast<size_t>(spec.width) * spec.height));
		}

		auto typedesc = compressed::enums::get_type_desc<T>();
		input_ptr->read_image(subimage, 0, 0, spec.nchannels, typedesc, static_cast<void*>(pixels.data()));
		compressed::image_algo::deinterleave(std::span<const T>(pixels), channels);
		return channels;
	}


	/// Compare two nested vectors (representing two multi-channel images), ensuring their contents are equal.
	/// 
	/// This function checks if two images (stored as `std::vector<std::vector<T>>`) have the same number of channels, 
	/// that each channel contains the same number of elements, and that all pixel values match. 
	/// If any discrepancy is found, a detailed exception is thrown.
	/// 
	/// \tparam T The pixel data type (e.g., uint8_t, float).
	/// \param a The first image to compare.
	/// \param b The second image to compare.
	/// \param name A label for the images, used in error messages.
	/// \throws std::runtime_error if the images differ in structure or content.
	template <typename T>
	void compare_images(std::vector<std::vector<T>> a, std::vector<std::vector<T>> b, std::string name)
	{
		if (a.size() != b.size())
		{
			throw std::runtime_error(std::format("{}: Error while comparing images, mismatch in number of channels {} : {}", name, a.size(), b.size()));
		}

		for (auto channel_idx : std::views::iota(static_cast<size_t>(0), a.size()))
		{
			if (a[channel_idx].size() != b[channel_idx].size())
			{
				throw std::runtime_error(
					std::format("{}: Error while comparing images, mismatch in number of items while comparing channel {} a: {:L} b: {:L}",
						name,
						channel_idx,
						a[channel_idx].size(),
						b[channel_idx].size())
				);
			}

			for (auto i : std::views::iota(static_cast<size_t>(0), a[channel_idx].size()))
			{
				if (a[channel_idx][i] != b[channel_idx][i])
				{
					throw std::runtime_error(
						std::format("{}: Error while comparing images, mismatch at element {} in channel {}. a: {}, b: {}",
							name,
							i,
							channel_idx,
							a[channel_idx][i],
							b[channel_idx][i])
					);
				}
			}
		}
	}


	/// Parametrize the given test lambda for the given types.
	template <typename... Types, typename Lambda>
	void parametrize(Lambda&& lambda)
	{
		(lambda(Types{}), ...);
	}

	namespace detail
	{
		template <typename Container1, typename Container2>
		concept ContainerPair = requires(Container1 x, Container2 y) {
			{ x.size() } -> std::convertible_to<std::size_t>;
			{ y.size() } -> std::convertible_to<std::size_t>;
			{ x[0] } -> std::same_as<decltype(y[0])>;
		};

		template <typename Container, typename T>
		concept ContainerAndValue = requires(Container x, T val) {
			{ x.size() } -> std::convertible_to<std::size_t>;
			{ x[0] != val } -> std::convertible_to<bool>;
		};

	} // detail


	template <typename Container1, typename Container2>
		requires detail::ContainerPair<Container1, Container2>
	void check_vector_verbose(const Container1& x, const Container2& y) 
	{
		REQUIRE(x.size() == y.size());
		for (size_t i = 0; i < x.size(); ++i) {
			if (x[i] != y[i]) {
				REQUIRE_MESSAGE(x[i] == y[i], "Failed vector index: " << i);
			}
		}
	}

	template <typename Container, typename T>
		requires detail::ContainerAndValue<Container, T>
	void check_vector_verbose(const Container& x, T y)
	{
		for (size_t i = 0; i < x.size(); ++i) {
			if (x[i] != y) {
				REQUIRE_MESSAGE(x[i] == y, "Failed vector index: " << i);
			}
		}
	}

}