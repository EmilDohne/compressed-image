#pragma once

#include <map>

#include "macros.h"

#ifdef COMPRESSED_IMAGE_OIIO_AVAILABLE
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/half.h>
#endif

namespace
NAMESPACE_COMPRESSED_IMAGE
{
    namespace enums
    {
        /// Enum representing available compression codecs.
        ///
        /// These codecs are inherited from `blosc2`/`nvcomp` and define different compression algorithms
        /// that can be used when storing compressed images. Any gpu codecs rely on nvidia gpus to function
        /// but will fall back gracefully to a cpu equivalent should there be no nvidia gpu or missing cuda
        /// libraries.
        enum class codec
        {
            blosclz, ///< Lightweight, fast compression optimized for high-speed decompression.
            lz4, ///< Extremely fast compression and decompression with moderate compression ratio.
            lz4hc, ///< High-compression variant of LZ4 with slower compression but similar fast decompression.
            zstd, ///< Zstandard compression providing high compression ratios with decent speed.
            lz4_gpu, ///< (cuda) gpu variant of lz4 compression, faster throughput compared to regular lz4
            snappy_gpu, ///< (cuda) gpu variant of snappy, a fast compression codec with moderate throughput
            zstd_gpu, ///< (cuda) gpu variant of zstd, faster throughput compared to regular zstd
            deflate_gpu, ///< (cuda) gpu variant of deflate, faster througput compared to regular deflate
            gdeflate_gpu, ///< (cuda) a bit-swizzled variant of deflate, optimized for gpu performance.
            cascaded_gpu
            ///< (cuda) proprietary compression scheme built up by several simple compression schemes like rle, bitpacking and delta
        };

        [[nodiscard]] inline bool is_gpu_codec(const codec codec)
        {
            if (codec == codec::blosclz || codec == codec::lz4 || codec == codec::lz4hc || codec == codec::zstd)
            {
                return false;
            }
            return true;
        }

        /// \brief map for the cpu codec fallbacks if no nvidia gpu is detected.
        ///
        /// These are constant and do not change.
        static const std::map<codec, codec> s_gpu_codec_fallback = {
            {codec::lz4_gpu, codec::lz4},
            {codec::snappy_gpu, codec::lz4},
            {codec::zstd_gpu, codec::zstd},
            {codec::deflate_gpu, codec::zstd},
            {codec::gdeflate_gpu, codec::zstd},
            {codec::cascaded_gpu, codec::lz4}
        };

        namespace detail
        {
            /// \brief enum representing the different underlying compression/decompression wrappers we use for cpu/gpu
            enum class compression_library
            {
                c_blosc2,
                nvcomp
            };

            /// \brief mapping of compression codecs to their respective underlying libraries.
            ///
            /// Used internally to dispatch the calls.
            static const std::map<codec, compression_library> s_library_mapping = {
                {codec::blosclz, compression_library::c_blosc2},
                {codec::lz4, compression_library::c_blosc2},
                {codec::lz4hc, compression_library::c_blosc2},
                {codec::zstd, compression_library::c_blosc2},
                {codec::lz4_gpu, compression_library::nvcomp},
                {codec::snappy_gpu, compression_library::nvcomp},
                {codec::zstd_gpu, compression_library::nvcomp},
                {codec::deflate_gpu, compression_library::nvcomp},
                {codec::gdeflate_gpu, compression_library::nvcomp},
                {codec::cascaded_gpu, compression_library::nvcomp}
            };
        } // namespace detail


#ifdef COMPRESSED_IMAGE_OIIO_AVAILABLE

        /// Get a OpenImageIO TypeDesc based on the given template parameter returning OIIO::TypeDesc::Unknown
        /// if the image coordinate is not part of the valid template specializations for photoshop buffers
        template <typename T>
        constexpr OIIO::TypeDesc get_type_desc()
        {
            if constexpr (std::is_same_v<T, uint8_t>)
            {
                return OIIO::TypeDesc::UINT8;
            }
            else if constexpr (std::is_same_v<T, int8_t>)
            {
                return OIIO::TypeDesc::INT8;
            }
            else if constexpr (std::is_same_v<T, uint16_t>)
            {
                return OIIO::TypeDesc::UINT16;
            }
            else if constexpr (std::is_same_v<T, int16_t>)
            {
                return OIIO::TypeDesc::INT16;
            }
            else if constexpr (std::is_same_v<T, uint32_t>)
            {
                return OIIO::TypeDesc::UINT32;
            }
            else if constexpr (std::is_same_v<T, int32_t>)
            {
                return OIIO::TypeDesc::INT32;
            }
            else if constexpr (std::is_same_v<T, float>)
            {
                return OIIO::TypeDesc::FLOAT;
            }
            else if constexpr (std::is_same_v<T, half>)
            {
                return OIIO::TypeDesc::HALF;
            }
            else
            {
                return OIIO::TypeDesc::UNKNOWN;
            }
        }

#endif // COMPRESSED_IMAGE_OIIO_AVAILABLE
    } // namespace enums
} // NAMESPACE_COMPRESSED_IMAGE
