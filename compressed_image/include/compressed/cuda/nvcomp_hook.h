#pragma once

#include <string>

#include <nvcomp.h>
#include <nvcomp/lz4.h>
#include <nvcomp/cascaded.h>
#include <nvcomp/deflate.h>
#include <nvcomp/gdeflate.h>
#include <nvcomp/snappy.h>
#include <nvcomp/zstd.h>

#include "compressed/macros.h"
#include "compressed/cuda/proc_util.h"

namespace
NAMESPACE_COMPRESSED_IMAGE::cuda
{
    /// \brief Singleton class for dynamically loading nvcomp functions and constants at runtime.
    class nvcomp_api
    {
    public:
        static nvcomp_api& instance()
        {
            static nvcomp_api inst;
            return inst;
        }

        bool available() const noexcept { return handle_ != nullptr; }

        // --- LZ4 API ---
        nvcompStatus_t LZ4CompressGetTempSizeAsync(size_t n,
                                                   size_t s,
                                                   nvcompBatchedLZ4CompressOpts_t o,
                                                   size_t* t,
                                                   size_t m) const { return lz4_comp_temp_fn_(n, s, o, t, m); }

        nvcompStatus_t LZ4DecompressGetTempSizeAsync(size_t n,
                                                     size_t s,
                                                     nvcompBatchedLZ4DecompressOpts_t o,
                                                     size_t* t,
                                                     size_t m) const { return lz4_decomp_temp_fn_(n, s, o, t, m); }

        nvcompStatus_t LZ4CompressGetMaxOutputChunkSize(size_t s, nvcompBatchedLZ4CompressOpts_t o, size_t* m) const
        {
            return lz4_max_out_fn_(s, o, m);
        }

        nvcompStatus_t LZ4CompressAsync(const void* const* u,
                                        const size_t* us,
                                        size_t ms,
                                        size_t b,
                                        void* tp,
                                        size_t tb,
                                        void* const* cp,
                                        size_t* cs,
                                        nvcompBatchedLZ4CompressOpts_t o,
                                        nvcompStatus_t* st,
                                        cudaStream_t stream) const
        {
            return lz4_compress_fn_(u, us, ms, b, tp, tb, cp, cs, o, st, stream);
        }

        nvcompStatus_t LZ4DecompressAsync(const void* const* cp,
                                          const size_t* cs,
                                          const size_t* us,
                                          size_t* ac,
                                          size_t b,
                                          void* tp,
                                          size_t tb,
                                          void* const* up,
                                          nvcompBatchedLZ4DecompressOpts_t o,
                                          nvcompStatus_t* st,
                                          cudaStream_t stream) const
        {
            return lz4_decompress_fn_(cp, cs, us, ac, b, tp, tb, up, o, st, stream);
        }

        // --- Cascaded API ---
        nvcompStatus_t CascadedCompressGetTempSizeAsync(size_t n,
                                                        size_t s,
                                                        nvcompBatchedCascadedCompressOpts_t o,
                                                        size_t* t,
                                                        size_t m) const
        {
            return cascaded_comp_temp_fn_(n, s, o, t, m);
        }

        nvcompStatus_t CascadedDecompressGetTempSizeAsync(size_t n,
                                                          size_t s,
                                                          nvcompBatchedCascadedDecompressOpts_t o,
                                                          size_t* t,
                                                          size_t m) const
        {
            return cascaded_decomp_temp_fn_(n, s, o, t, m);
        }

        nvcompStatus_t CascadedCompressGetMaxOutputChunkSize(size_t s,
                                                             nvcompBatchedCascadedCompressOpts_t o,
                                                             size_t* m) const { return cascaded_max_out_fn_(s, o, m); }

        nvcompStatus_t CascadedCompressAsync(const void* const* u,
                                             const size_t* us,
                                             size_t ms,
                                             size_t b,
                                             void* tp,
                                             size_t tb,
                                             void* const* cp,
                                             size_t* cs,
                                             nvcompBatchedCascadedCompressOpts_t o,
                                             nvcompStatus_t* st,
                                             cudaStream_t stream) const
        {
            return cascaded_compress_fn_(u, us, ms, b, tp, tb, cp, cs, o, st, stream);
        }

        nvcompStatus_t CascadedDecompressAsync(const void* const* cp,
                                               const size_t* cs,
                                               const size_t* us,
                                               size_t* ac,
                                               size_t b,
                                               void* tp,
                                               size_t tb,
                                               void* const* up,
                                               nvcompBatchedCascadedDecompressOpts_t o,
                                               nvcompStatus_t* st,
                                               cudaStream_t stream) const
        {
            return cascaded_decompress_fn_(cp, cs, us, ac, b, tp, tb, up, o, st, stream);
        }

        // --- Deflate API ---
        nvcompStatus_t DeflateCompressGetTempSizeAsync(size_t n,
                                                       size_t s,
                                                       nvcompBatchedDeflateCompressOpts_t o,
                                                       size_t* t,
                                                       size_t m) const { return deflate_comp_temp_fn_(n, s, o, t, m); }

        nvcompStatus_t DeflateDecompressGetTempSizeAsync(size_t n,
                                                         size_t s,
                                                         nvcompBatchedDeflateDecompressOpts_t o,
                                                         size_t* t,
                                                         size_t m) const
        {
            return deflate_decomp_temp_fn_(n, s, o, t, m);
        }

        nvcompStatus_t DeflateCompressGetMaxOutputChunkSize(size_t s,
                                                            nvcompBatchedDeflateCompressOpts_t o,
                                                            size_t* m) const { return deflate_max_out_fn_(s, o, m); }

        nvcompStatus_t DeflateCompressAsync(const void* const* u,
                                            const size_t* us,
                                            size_t ms,
                                            size_t b,
                                            void* tp,
                                            size_t tb,
                                            void* const* cp,
                                            size_t* cs,
                                            nvcompBatchedDeflateCompressOpts_t o,
                                            nvcompStatus_t* st,
                                            cudaStream_t stream) const
        {
            return deflate_compress_fn_(u, us, ms, b, tp, tb, cp, cs, o, st, stream);
        }

        nvcompStatus_t DeflateDecompressAsync(const void* const* cp,
                                              const size_t* cs,
                                              const size_t* us,
                                              size_t* ac,
                                              size_t b,
                                              void* tp,
                                              size_t tb,
                                              void* const* up,
                                              nvcompBatchedDeflateDecompressOpts_t o,
                                              nvcompStatus_t* st,
                                              cudaStream_t stream) const
        {
            return deflate_decompress_fn_(cp, cs, us, ac, b, tp, tb, up, o, st, stream);
        }

        // --- Gdeflate API ---
        nvcompStatus_t GdeflateCompressGetTempSizeAsync(size_t n,
                                                        size_t s,
                                                        nvcompBatchedGdeflateCompressOpts_t o,
                                                        size_t* t,
                                                        size_t m) const
        {
            return gdeflate_comp_temp_fn_(n, s, o, t, m);
        }

        nvcompStatus_t GdeflateDecompressGetTempSizeAsync(size_t n,
                                                          size_t s,
                                                          nvcompBatchedGdeflateDecompressOpts_t o,
                                                          size_t* t,
                                                          size_t m) const
        {
            return gdeflate_decomp_temp_fn_(n, s, o, t, m);
        }

        nvcompStatus_t GdeflateCompressGetMaxOutputChunkSize(size_t s,
                                                             nvcompBatchedGdeflateCompressOpts_t o,
                                                             size_t* m) const { return gdeflate_max_out_fn_(s, o, m); }

        nvcompStatus_t GdeflateCompressAsync(const void* const* u,
                                             const size_t* us,
                                             size_t ms,
                                             size_t b,
                                             void* tp,
                                             size_t tb,
                                             void* const* cp,
                                             size_t* cs,
                                             nvcompBatchedGdeflateCompressOpts_t o,
                                             nvcompStatus_t* st,
                                             cudaStream_t stream) const
        {
            return gdeflate_compress_fn_(u, us, ms, b, tp, tb, cp, cs, o, st, stream);
        }

        nvcompStatus_t GdeflateDecompressAsync(const void* const* cp,
                                               const size_t* cs,
                                               const size_t* us,
                                               size_t* ac,
                                               size_t b,
                                               void* tp,
                                               size_t tb,
                                               void* const* up,
                                               nvcompBatchedGdeflateDecompressOpts_t o,
                                               nvcompStatus_t* st,
                                               cudaStream_t stream) const
        {
            return gdeflate_decompress_fn_(cp, cs, us, ac, b, tp, tb, up, o, st, stream);
        }

        // --- Snappy API ---
        nvcompStatus_t SnappyCompressGetTempSizeAsync(size_t n,
                                                      size_t s,
                                                      nvcompBatchedSnappyCompressOpts_t o,
                                                      size_t* t,
                                                      size_t m) const { return snappy_comp_temp_fn_(n, s, o, t, m); }

        nvcompStatus_t SnappyDecompressGetTempSizeAsync(size_t n,
                                                        size_t s,
                                                        nvcompBatchedSnappyDecompressOpts_t o,
                                                        size_t* t,
                                                        size_t m) const
        {
            return snappy_decomp_temp_fn_(n, s, o, t, m);
        }

        nvcompStatus_t SnappyCompressGetMaxOutputChunkSize(size_t s,
                                                           nvcompBatchedSnappyCompressOpts_t o,
                                                           size_t* m) const { return snappy_max_out_fn_(s, o, m); }

        nvcompStatus_t SnappyCompressAsync(const void* const* u,
                                           const size_t* us,
                                           size_t ms,
                                           size_t b,
                                           void* tp,
                                           size_t tb,
                                           void* const* cp,
                                           size_t* cs,
                                           nvcompBatchedSnappyCompressOpts_t o,
                                           nvcompStatus_t* st,
                                           cudaStream_t stream) const
        {
            return snappy_compress_fn_(u, us, ms, b, tp, tb, cp, cs, o, st, stream);
        }

        nvcompStatus_t SnappyDecompressAsync(const void* const* cp,
                                             const size_t* cs,
                                             const size_t* us,
                                             size_t* ac,
                                             size_t b,
                                             void* tp,
                                             size_t tb,
                                             void* const* up,
                                             nvcompBatchedSnappyDecompressOpts_t o,
                                             nvcompStatus_t* st,
                                             cudaStream_t stream) const
        {
            return snappy_decompress_fn_(cp, cs, us, ac, b, tp, tb, up, o, st, stream);
        }

        // --- Zstd API ---
        nvcompStatus_t ZstdCompressGetTempSizeAsync(size_t n,
                                                    size_t s,
                                                    nvcompBatchedZstdCompressOpts_t o,
                                                    size_t* t,
                                                    size_t m) const { return zstd_comp_temp_fn_(n, s, o, t, m); }

        nvcompStatus_t ZstdDecompressGetTempSizeAsync(size_t n,
                                                      size_t s,
                                                      nvcompBatchedZstdDecompressOpts_t o,
                                                      size_t* t,
                                                      size_t m) const { return zstd_decomp_temp_fn_(n, s, o, t, m); }

        nvcompStatus_t ZstdCompressGetMaxOutputChunkSize(size_t s, nvcompBatchedZstdCompressOpts_t o, size_t* m) const
        {
            return zstd_max_out_fn_(s, o, m);
        }

        nvcompStatus_t ZstdCompressAsync(const void* const* u,
                                         const size_t* us,
                                         size_t ms,
                                         size_t b,
                                         void* tp,
                                         size_t tb,
                                         void* const* cp,
                                         size_t* cs,
                                         nvcompBatchedZstdCompressOpts_t o,
                                         nvcompStatus_t* st,
                                         cudaStream_t stream) const
        {
            return zstd_compress_fn_(u, us, ms, b, tp, tb, cp, cs, o, st, stream);
        }

        nvcompStatus_t ZstdDecompressAsync(const void* const* cp,
                                           const size_t* cs,
                                           const size_t* us,
                                           size_t* ac,
                                           size_t b,
                                           void* tp,
                                           size_t tb,
                                           void* const* up,
                                           nvcompBatchedZstdDecompressOpts_t o,
                                           nvcompStatus_t* st,
                                           cudaStream_t stream) const
        {
            return zstd_decompress_fn_(cp, cs, us, ac, b, tp, tb, up, o, st, stream);
        }

        nvcomp_api(const nvcomp_api&) = delete;
        nvcomp_api& operator=(const nvcomp_api&) = delete;

    private:
        nvcomp_api()
        {
#if defined(_WIN32)
            const std::string lib_name = "nvcomp64_5.dll";
#elif defined(__linux__)
            const std::string lib_name = "libnvcomp.so";
#else
            return;
#endif
            handle_ = proc::load_library(lib_name);
            if (!handle_) return;

#define LOAD_FN(fn, member) member = proc::get_symbol<decltype(member)>(handle_, #fn, lib_name)
#define LOAD_VAR(type, name, member) member = proc::get_symbol<type*>(handle_, #name, lib_name)

            // --- LZ4 ---
            LOAD_FN(nvcompBatchedLZ4CompressGetTempSizeAsync, lz4_comp_temp_fn_);
            LOAD_FN(nvcompBatchedLZ4DecompressGetTempSizeAsync, lz4_decomp_temp_fn_);
            LOAD_FN(nvcompBatchedLZ4CompressGetMaxOutputChunkSize, lz4_max_out_fn_);
            LOAD_FN(nvcompBatchedLZ4CompressAsync, lz4_compress_fn_);
            LOAD_FN(nvcompBatchedLZ4DecompressAsync, lz4_decompress_fn_);

            // --- Cascaded ---
            LOAD_FN(nvcompBatchedCascadedCompressGetTempSizeAsync, cascaded_comp_temp_fn_);
            LOAD_FN(nvcompBatchedCascadedDecompressGetTempSizeAsync, cascaded_decomp_temp_fn_);
            LOAD_FN(nvcompBatchedCascadedCompressGetMaxOutputChunkSize, cascaded_max_out_fn_);
            LOAD_FN(nvcompBatchedCascadedCompressAsync, cascaded_compress_fn_);
            LOAD_FN(nvcompBatchedCascadedDecompressAsync, cascaded_decompress_fn_);

            // --- Deflate ---
            LOAD_FN(nvcompBatchedDeflateCompressGetTempSizeAsync, deflate_comp_temp_fn_);
            LOAD_FN(nvcompBatchedDeflateDecompressGetTempSizeAsync, deflate_decomp_temp_fn_);
            LOAD_FN(nvcompBatchedDeflateCompressGetMaxOutputChunkSize, deflate_max_out_fn_);
            LOAD_FN(nvcompBatchedDeflateCompressAsync, deflate_compress_fn_);
            LOAD_FN(nvcompBatchedDeflateDecompressAsync, deflate_decompress_fn_);

            // --- Gdeflate ---
            LOAD_FN(nvcompBatchedGdeflateCompressGetTempSizeAsync, gdeflate_comp_temp_fn_);
            LOAD_FN(nvcompBatchedGdeflateDecompressGetTempSizeAsync, gdeflate_decomp_temp_fn_);
            LOAD_FN(nvcompBatchedGdeflateCompressGetMaxOutputChunkSize, gdeflate_max_out_fn_);
            LOAD_FN(nvcompBatchedGdeflateCompressAsync, gdeflate_compress_fn_);
            LOAD_FN(nvcompBatchedGdeflateDecompressAsync, gdeflate_decompress_fn_);

            // --- Snappy ---
            LOAD_FN(nvcompBatchedSnappyCompressGetTempSizeAsync, snappy_comp_temp_fn_);
            LOAD_FN(nvcompBatchedSnappyDecompressGetTempSizeAsync, snappy_decomp_temp_fn_);
            LOAD_FN(nvcompBatchedSnappyCompressGetMaxOutputChunkSize, snappy_max_out_fn_);
            LOAD_FN(nvcompBatchedSnappyCompressAsync, snappy_compress_fn_);
            LOAD_FN(nvcompBatchedSnappyDecompressAsync, snappy_decompress_fn_);

            // --- Zstd ---
            LOAD_FN(nvcompBatchedZstdCompressGetTempSizeAsync, zstd_comp_temp_fn_);
            LOAD_FN(nvcompBatchedZstdDecompressGetTempSizeAsync, zstd_decomp_temp_fn_);
            LOAD_FN(nvcompBatchedZstdCompressGetMaxOutputChunkSize, zstd_max_out_fn_);
            LOAD_FN(nvcompBatchedZstdCompressAsync, zstd_compress_fn_);
            LOAD_FN(nvcompBatchedZstdDecompressAsync, zstd_decompress_fn_);

#undef LOAD_FN
#undef LOAD_VAR
        }

        proc::library_handle handle_ = nullptr;

        // --- Function Pointer Types ---

        // Type definitions matching exact function layouts
        nvcompStatus_t (*lz4_comp_temp_fn_)(size_t, size_t, nvcompBatchedLZ4CompressOpts_t, size_t*, size_t) = nullptr;
        nvcompStatus_t (*lz4_decomp_temp_fn_)(size_t, size_t, nvcompBatchedLZ4DecompressOpts_t, size_t*, size_t) =
            nullptr;
        nvcompStatus_t (*lz4_max_out_fn_)(size_t, nvcompBatchedLZ4CompressOpts_t, size_t*) = nullptr;
        nvcompStatus_t (*lz4_compress_fn_)(const void* const*,
                                           const size_t*,
                                           size_t,
                                           size_t,
                                           void*,
                                           size_t,
                                           void* const*,
                                           size_t*,
                                           nvcompBatchedLZ4CompressOpts_t,
                                           nvcompStatus_t*,
                                           cudaStream_t) = nullptr;
        nvcompStatus_t (*lz4_decompress_fn_)(const void* const*,
                                             const size_t*,
                                             const size_t*,
                                             size_t*,
                                             size_t,
                                             void*,
                                             size_t,
                                             void* const*,
                                             nvcompBatchedLZ4DecompressOpts_t,
                                             nvcompStatus_t*,
                                             cudaStream_t) = nullptr;

        nvcompStatus_t (*cascaded_comp_temp_fn_)(size_t, size_t, nvcompBatchedCascadedCompressOpts_t, size_t*, size_t) =
            nullptr;
        nvcompStatus_t (*cascaded_decomp_temp_fn_)(size_t,
                                                   size_t,
                                                   nvcompBatchedCascadedDecompressOpts_t,
                                                   size_t*,
                                                   size_t) = nullptr;
        nvcompStatus_t (*cascaded_max_out_fn_)(size_t, nvcompBatchedCascadedCompressOpts_t, size_t*) = nullptr;
        nvcompStatus_t (*cascaded_compress_fn_)(const void* const*,
                                                const size_t*,
                                                size_t,
                                                size_t,
                                                void*,
                                                size_t,
                                                void* const*,
                                                size_t*,
                                                nvcompBatchedCascadedCompressOpts_t,
                                                nvcompStatus_t*,
                                                cudaStream_t) = nullptr;
        nvcompStatus_t (*cascaded_decompress_fn_)(const void* const*,
                                                  const size_t*,
                                                  const size_t*,
                                                  size_t*,
                                                  size_t,
                                                  void*,
                                                  size_t,
                                                  void* const*,
                                                  nvcompBatchedCascadedDecompressOpts_t,
                                                  nvcompStatus_t*,
                                                  cudaStream_t) = nullptr;

        nvcompStatus_t (*deflate_comp_temp_fn_)(size_t, size_t, nvcompBatchedDeflateCompressOpts_t, size_t*, size_t) =
            nullptr;
        nvcompStatus_t (*deflate_decomp_temp_fn_)(size_t, size_t, nvcompBatchedDeflateDecompressOpts_t, size_t*, size_t)
            = nullptr;
        nvcompStatus_t (*deflate_max_out_fn_)(size_t, nvcompBatchedDeflateCompressOpts_t, size_t*) = nullptr;
        nvcompStatus_t (*deflate_compress_fn_)(const void* const*,
                                               const size_t*,
                                               size_t,
                                               size_t,
                                               void*,
                                               size_t,
                                               void* const*,
                                               size_t*,
                                               nvcompBatchedDeflateCompressOpts_t,
                                               nvcompStatus_t*,
                                               cudaStream_t) = nullptr;
        nvcompStatus_t (*deflate_decompress_fn_)(const void* const*,
                                                 const size_t*,
                                                 const size_t*,
                                                 size_t*,
                                                 size_t,
                                                 void*,
                                                 size_t,
                                                 void* const*,
                                                 nvcompBatchedDeflateDecompressOpts_t,
                                                 nvcompStatus_t*,
                                                 cudaStream_t) = nullptr;

        nvcompStatus_t (*gdeflate_comp_temp_fn_)(size_t, size_t, nvcompBatchedGdeflateCompressOpts_t, size_t*, size_t) =
            nullptr;
        nvcompStatus_t (*gdeflate_decomp_temp_fn_)(size_t,
                                                   size_t,
                                                   nvcompBatchedGdeflateDecompressOpts_t,
                                                   size_t*,
                                                   size_t) = nullptr;
        nvcompStatus_t (*gdeflate_max_out_fn_)(size_t, nvcompBatchedGdeflateCompressOpts_t, size_t*) = nullptr;
        nvcompStatus_t (*gdeflate_compress_fn_)(const void* const*,
                                                const size_t*,
                                                size_t,
                                                size_t,
                                                void*,
                                                size_t,
                                                void* const*,
                                                size_t*,
                                                nvcompBatchedGdeflateCompressOpts_t,
                                                nvcompStatus_t*,
                                                cudaStream_t) = nullptr;
        nvcompStatus_t (*gdeflate_decompress_fn_)(const void* const*,
                                                  const size_t*,
                                                  const size_t*,
                                                  size_t*,
                                                  size_t,
                                                  void*,
                                                  size_t,
                                                  void* const*,
                                                  nvcompBatchedGdeflateDecompressOpts_t,
                                                  nvcompStatus_t*,
                                                  cudaStream_t) = nullptr;

        nvcompStatus_t (*snappy_comp_temp_fn_)(size_t, size_t, nvcompBatchedSnappyCompressOpts_t, size_t*, size_t) =
            nullptr;
        nvcompStatus_t (*snappy_decomp_temp_fn_)(size_t, size_t, nvcompBatchedSnappyDecompressOpts_t, size_t*, size_t) =
            nullptr;
        nvcompStatus_t (*snappy_max_out_fn_)(size_t, nvcompBatchedSnappyCompressOpts_t, size_t*) = nullptr;
        nvcompStatus_t (*snappy_compress_fn_)(const void* const*,
                                              const size_t*,
                                              size_t,
                                              size_t,
                                              void*,
                                              size_t,
                                              void* const*,
                                              size_t*,
                                              nvcompBatchedSnappyCompressOpts_t,
                                              nvcompStatus_t*,
                                              cudaStream_t) = nullptr;
        nvcompStatus_t (*snappy_decompress_fn_)(const void* const*,
                                                const size_t*,
                                                const size_t*,
                                                size_t*,
                                                size_t,
                                                void*,
                                                size_t,
                                                void* const*,
                                                nvcompBatchedSnappyDecompressOpts_t,
                                                nvcompStatus_t*,
                                                cudaStream_t) = nullptr;

        nvcompStatus_t (*zstd_comp_temp_fn_)(size_t, size_t, nvcompBatchedZstdCompressOpts_t, size_t*, size_t) =
            nullptr;
        nvcompStatus_t (*zstd_decomp_temp_fn_)(size_t, size_t, nvcompBatchedZstdDecompressOpts_t, size_t*, size_t) =
            nullptr;
        nvcompStatus_t (*zstd_max_out_fn_)(size_t, nvcompBatchedZstdCompressOpts_t, size_t*) = nullptr;
        nvcompStatus_t (*zstd_compress_fn_)(const void* const*,
                                            const size_t*,
                                            size_t,
                                            size_t,
                                            void*,
                                            size_t,
                                            void* const*,
                                            size_t*,
                                            nvcompBatchedZstdCompressOpts_t,
                                            nvcompStatus_t*,
                                            cudaStream_t) = nullptr;
        nvcompStatus_t (*zstd_decompress_fn_)(const void* const*,
                                              const size_t*,
                                              const size_t*,
                                              size_t*,
                                              size_t,
                                              void*,
                                              size_t,
                                              void* const*,
                                              nvcompBatchedZstdDecompressOpts_t,
                                              nvcompStatus_t*,
                                              cudaStream_t) = nullptr;
    };
}
