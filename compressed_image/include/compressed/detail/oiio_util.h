#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <vector>
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



        // Utilities related to OIIO ParamValue (the internal metadata type) helping us convert them into json-able
        // types
        namespace param_value
        {

            /// \brief JSON-like types that we can store 
            enum class _JSONType
            {
                _int,
                _float,
                _string
            };

            inline _JSONType to_json_type(const OIIO::ParamValue& pvalue)
            {
                auto type = pvalue.type();


                if (type == OIIO::TypeDesc::STRING || type == OIIO::TypeDesc::USTRINGHASH)
                {
                    return _JSONType::_string;
                }

                if (
                    type == OIIO::TypeDesc::UINT8 ||
                    type == OIIO::TypeDesc::UCHAR ||
                    type == OIIO::TypeDesc::INT8 ||
                    type == OIIO::TypeDesc::CHAR ||
                    type == OIIO::TypeDesc::UINT16 ||
                    type == OIIO::TypeDesc::USHORT ||
                    type == OIIO::TypeDesc::INT16 ||
                    type == OIIO::TypeDesc::SHORT ||
                    type == OIIO::TypeDesc::UINT32 ||
                    type == OIIO::TypeDesc::UINT ||
                    type == OIIO::TypeDesc::INT32 ||
                    type == OIIO::TypeDesc::INT ||
                    type == OIIO::TypeDesc::UINT64 ||
                    type == OIIO::TypeDesc::ULONGLONG ||
                    type == OIIO::TypeDesc::INT64 ||
                    type == OIIO::TypeDesc::LONGLONG
                    )
                {
                    return _JSONType::_int;
                }

                if (
                    type == OIIO::TypeDesc::HALF ||
                    type == OIIO::TypeDesc::FLOAT ||
                    type == OIIO::TypeDesc::DOUBLE
                    )
                {
                    return _JSONType::_float;
                }

                throw std::invalid_argument(
                    std::format(
                        "Unknown json type for param value: {}", pvalue.name().string()
                    )
                );
            }

            inline bool is_array(const OIIO::ParamValue& pvalue)
            {
                return pvalue.nvalues() > 1;
            }

            template <typename T>
                requires std::is_same_v<T, std::string> || std::is_same_v<T, int> || std::is_same_v<T, float>
            T decode(const OIIO::ParamValue& pvalue)
            {
                if constexpr (std::is_same_v<T, std::string>)
                {
                    return pvalue.get_string();
                }
                else if constexpr (std::is_same_v<T, int>)
                {
                    return pvalue.get_int();
                }
                else
                {
                    return pvalue.get_float();
                }
            }


            template <typename T>
                requires std::is_same_v<T, std::string> || std::is_same_v<T, int> || std::is_same_v<T, float>
            std::vector<T> decode_array(const OIIO::ParamValue& pvalue)
            {
                std::vector<T> out{};
                for (auto i : std::views::iota(0, pvalue.nvalues()))
                {
                    if constexpr (std::is_same_v<T, std::string>)
                    {
                        out.push_back(pvalue.get_string_indexed(i));
                    }
                    else if constexpr (std::is_same_v<T, int>)
                    {
                        out.push_back(pvalue.get_int_indexed(i));
                    }
                    else
                    {
                        out.push_back(pvalue.get_float_indexed(i));
                    }
                }
                return out;
            }


            inline json_ordered to_json(const OIIO::ParamValueList& list)
            {
                json_ordered out{};

                for (const auto& pvalue : list)
                {
                    const auto& name = pvalue.name().string();
                    _JSONType json_type = _JSONType::_string;
                    try
                    {
                        json_type = param_value::to_json_type(pvalue);
                    }
                    catch ([[maybe_unused]] const std::invalid_argument& e)
                    {
                        continue;
                    }
                    auto is_p_array = param_value::is_array(pvalue);


                    if (json_type == _JSONType::_string)
                    {
                        if (is_p_array)
                        {
                            out[name] = param_value::decode_array<std::string>(pvalue);
                        }
                        else
                        {
                            out[name] = param_value::decode<std::string>(pvalue);
                        }
                    }
                    else if (json_type == _JSONType::_int)
                    {
                        if (is_p_array)
                        {
                            out[name] = param_value::decode_array<int>(pvalue);
                        }
                        else
                        {
                            out[name] = param_value::decode<int>(pvalue);
                        }
                    }
                    else if (json_type == _JSONType::_float)
                    {
                        if (is_p_array)
                        {
                            out[name] = param_value::decode_array<float>(pvalue);
                        }
                        else
                        {
                            out[name] = param_value::decode<float>(pvalue);
                        }
                    }
                }

                return out;
            }
        }

    } // detail


} // NAMESPACE_COMPRESSED_IMAGE

#endif // COMPRESSED_IMAGE_OIIO_AVAILABLE