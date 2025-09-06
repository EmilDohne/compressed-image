#pragma once

#include "compressed/macros.h"

#include "compressed/cuda/cuda_hook.h"


namespace NAMESPACE_COMPRESSED_IMAGE
{

	namespace cuda
	{

		/// \brief Check if CUDA runtime is available and at least one device exists.
		inline bool is_available()
		{
			return cuda_api::instance().available() && cuda_api::instance().has_device();
		}

		/// \brief Get the number of available CUDA devices.
		/// \return Number of devices detected by the CUDA runtime.
		inline int device_count()
		{
			return cuda_api::instance().device_count();
		}

		/// \brief Get the index of the currently active CUDA device.
		/// \return Device index (0-based).
		inline int current_device()
		{
			return cuda_api::instance().current_device();
		}

		/// \brief Set the active CUDA device for the calling thread.
		/// \param device The index of the device to make current.
		inline void set_device(int device)
		{
			cuda_api::instance().set_device(device);
		}

		/// \brief Retrieve full device property structures for all CUDA devices.
		/// \return A vector of \c cudaDeviceProp, one for each device.
		inline std::vector<cudaDeviceProp> devices()
		{
			std::vector<cudaDeviceProp> properties;
			for (int i = 0; i < device_count(); ++i)
			{
				properties.push_back(cuda_api::instance().device_properties(i));
			}
			return properties;
		}

		/// \brief Get the names of all available CUDA devices.
		/// \return A vector of device name strings.
		inline std::vector<std::string> device_names()
		{
			std::vector<std::string> names;
			for (int i = 0; i < device_count(); ++i)
			{
				names.emplace_back(cuda_api::instance().device_properties(i).name);
			}
			return names;
		}

		/// \brief RAII guard to temporarily switch CUDA devices.
		///
		/// Saves the currently active device on construction, switches to the given
		/// device, and restores the previous device on destruction.
		struct device_guard
		{

			/// \brief Construct a guard and switch to the given device.
			/// \param new_device The device index to switch to.
			explicit device_guard(int new_device)
			{
				prev_device_ = current_device();
				set_device(new_device);
			}

			/// \brief Destructor restores the previous device.
			~device_guard()
			{
				try { set_device(prev_device_); }
				catch (...) {}
			}

			device_guard(const device_guard&) = delete;
			device_guard& operator=(const device_guard&) = delete;

		private:
			int prev_device_{ -1 }; ///< Previously active device index.
		};


	} // namespace cuda

} // namespace NAMESPACE_COMPRESSED_IMAGE