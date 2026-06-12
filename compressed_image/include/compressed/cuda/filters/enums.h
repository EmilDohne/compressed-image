#pragma once

#include <string_view>

#include "compressed/macros.h"

namespace
NAMESPACE_COMPRESSED_IMAGE
{
    namespace cuda
    {
        namespace enums
        {
            enum class filter
            {
                bytedelta,
                delta,
                xordelta,
                shuffle,
                fmap,
            };

            constexpr std::string_view to_string(const filter _filter)
            {
                switch (_filter)
                {
                case filter::bytedelta: return "bytedelta";
                case filter::delta: return "delta";
                case filter::xordelta: return "xordelta";
                case filter::shuffle: return "shuffle";
                case filter::fmap: return "fmap";
                default: return "unknown";
                }
            }
        } // namespace enums
    } // namespace cuda
} // namespace NAMESPACE_COMPRESSED_IMAGE
