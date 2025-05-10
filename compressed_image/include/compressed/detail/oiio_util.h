#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <ranges>


#ifdef COMPRESSED_IMAGE_OIIO_AVAILABLE

#include <OpenImageIO/imageio.h>


namespace NAMESPACE_COMPRESSED_IMAGE
{

    namespace detail
    {

        /// \brief Create a mapping of contiguous begin-end pairs from the passed channel names
        /// 
        /// Takes the input channel names and constructs a list of (sorted) pairs for the begin and end channel ranges.
        /// So if we pass 'R', 'B' and 'A' in a rgba image we would get the following
        /// 
        /// { {0 - 1}, {2 - 4} }
        /// 
        /// which describe the channels 1, 2 and 3. 
        /// 
        /// \param input_ptr The image to query
        /// \param channelnames The channelnames to construct pairings for, invalid channelnames throw a std::out_of_range
        /// 
        /// \return A mapping of begin-end pairs for the channels
        inline std::vector<std::pair<int, int>>get_contiguous_channels(
            const std::unique_ptr<OIIO::ImageInput>& input_ptr,
            std::vector<std::string> channelnames
        )
        {
            std::unordered_map<std::string, int> map_name_to_index;
            for (auto index : std::views::iota(0, static_cast<int>(input_ptr->spec().channelnames.size())))
            {
                map_name_to_index[input_ptr->spec().channelnames.at(index)] = index;
            }

            // Get the indices from the channnelnames
            std::vector<int> indices;
            for (const auto& channelname : channelnames)
            {
                indices.push_back(map_name_to_index.at(channelname));
            }

            // Sort them to ensure we can map them correctly.
            std::sort(indices.begin(), indices.end());

            std::vector<std::pair<int, int>> result;
            if (indices.empty())
            {
                return result;
            }

            int range_start = indices[0];
            int previous = indices[0];

            for (size_t i = 1; i < indices.size(); ++i)
            {
                if (indices[i] != previous + 1)
                {
                    result.emplace_back(range_start, previous + 1);
                    range_start = indices[i];
                }
                previous = indices[i];
            }
            result.emplace_back(range_start, previous + 1);

            return result;
        }

    } // detail


} // NAMESPACE_COMPRESSED_IMAGE

#endif // COMPRESSED_IMAGE_OIIO_AVAILABLE