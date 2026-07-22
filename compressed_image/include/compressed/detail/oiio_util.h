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

#include "scoped_timer.h"
#include "compressed/json_alias.h"

namespace
NAMESPACE_COMPRESSED_IMAGE
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
        /// This function assumes that the subimage is set appropriately on the image before the call to this function!
        /// 
        /// \param input_ptr The image to query
        /// \param channelnames The channelnames to construct pairings for, invalid channelnames throw a std::out_of_range
        /// 
        /// \return A mapping of begin-end pairs for the channels
        inline std::vector<std::pair<int, int>> get_contiguous_channels(
            const std::unique_ptr<OIIO::ImageInput>& input_ptr,
            const std::vector<std::string> channelnames
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
            std::ranges::sort(indices);

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
                _COMPRESSED_PROFILE_FUNCTION();
                auto type = pvalue.type();


                if (type == OIIO::TypeDesc::STRING || type == OIIO::TypeDesc::USTRINGHASH)
                {
                    return _JSONType::_string;
                }
                else if (
                    type == OIIO::TypeDesc::UINT8 ||
                    type == OIIO::TypeDesc::INT8 ||
                    type == OIIO::TypeDesc::UINT16 ||
                    type == OIIO::TypeDesc::INT16 ||
                    type == OIIO::TypeDesc::UINT32 ||
                    type == OIIO::TypeDesc::INT32 ||
                    type == OIIO::TypeDesc::UINT64 ||
                    type == OIIO::TypeDesc::INT64
                )
                {
                    return _JSONType::_int;
                }
                else if (type == OIIO::TypeDesc::HALF || type == OIIO::TypeDesc::FLOAT || type ==
                    OIIO::TypeDesc::DOUBLE)
                {
                    return _JSONType::_float;
                }

                throw std::invalid_argument(
                    std::format(
                        "Unknown json type for param value: {}",
                        pvalue.name().string()
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
                _COMPRESSED_PROFILE_FUNCTION();
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
                _COMPRESSED_PROFILE_FUNCTION();
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
                _COMPRESSED_PROFILE_FUNCTION();
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

            /// \brief Write simple json metadata back onto an OIIO ImageSpec (inverse of \ref to_json).
            ///
            /// Only scalar string / integer / floating-point entries are written; array-valued and
            /// nested-object entries are skipped, so this is not a fully lossless inverse of `to_json`.
            ///
            /// \param spec     The spec to attach the attributes to.
            /// \param metadata The json metadata to encode.
            inline void from_json(OIIO::ImageSpec& spec, const json_ordered& metadata)
            {
                _COMPRESSED_PROFILE_FUNCTION();
                if (!metadata.is_object())
                {
                    return;
                }

                for (auto it = metadata.begin(); it != metadata.end(); ++it)
                {
                    const std::string& name = it.key();
                    const auto& value = it.value();

                    if (value.is_string())
                    {
                        spec.attribute(name, value.get<std::string>());
                    }
                    else if (value.is_number_integer() || value.is_number_unsigned())
                    {
                        spec.attribute(name, value.get<int>());
                    }
                    else if (value.is_number_float())
                    {
                        spec.attribute(name, static_cast<float>(value.get<double>()));
                    }
                    // Arrays and nested objects are intentionally not round-tripped (v1 limitation).
                }
            }
        }
    } // detail
} // NAMESPACE_COMPRESSED_IMAGE

#endif // COMPRESSED_IMAGE_OIIO_AVAILABLE
