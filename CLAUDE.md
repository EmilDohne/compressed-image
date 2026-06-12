# CLAUDE.md — compressed_image project context

GPU image-compression library. Per-channel compression of natural images and
CGI/VFX imagery (including AOVs: depth, normals, position, IDs, beauty passes)
through a filter pipeline followed by nvcomp batch compressors (zstd, lz4,
deflate, gdeflate, snappy, cascaded). Primary constraint: **compression
speed** (nvcomp kernels are ~98% of runtime; filters ~1.6%). Everything must
be strictly lossless (bit-exact, including NaN payloads, ±Inf, −0.0,
denormals).

## Architecture

- **Filter plugins**: each filter is a separately compiled CUDA shared library
  (`libcompressed_<name>_plugin.so` / `.dll`), dynamically loaded via a
  singleton hook header (`filter::<name>::instance()`, `available()`,
  `forward()`, `backward()`), all sharing the signature
  `cudaError_t fn(const uint8_t*, uint8_t*, size_t length, size_t typesize, cudaStream_t)`.
- **Pipeline**: `apply_forward_pipeline<T>` / `apply_backward_pipeline<T>`
  ping-pong between output and a temp buffer; backward iterates the filter
  list in reverse. Filters that fail to load are skipped with a warning.
- **Chunks**: `compressed_chunk<T>` stores compressed blocks (~64 KB nvcomp
  blocks), per-block sizes, the `nvcomp_context`, **the filter list used**
  (so per-chunk filter choices need no decoder changes), and an optional
  `constant_value` short-circuit for fully constant chunks (bitwise compare,
  checked on host before upload).
- Defaults come from `compressor<T>::get_default_filters()`, overridable per
  call via the `_filters` argument.

## Current per-type default pipelines (empirically settled)

| Type      | Pipeline                     | Notes                                                                                               |
|-----------|------------------------------|-----------------------------------------------------------------------------------------------------|
| uint8     | `delta`                      | zigzag pointless without a shuffle stage; consider benchmarking `{}` (no filter) for mask-like data |
| uint16    | `delta(+zigzag) + shuffle`   | zigzag inside the delta kernel; confirmed cratio win                                                |
| uint32    | `xordelta + shuffle`         | switched from `delta + shuffle` based on measured stats (see findings)                              |
| f16 / f32 | `fmap + shuffle + bytedelta` | **+0.1–0.3 cratio over previous `xordelta + shuffle`**                                              |

## Key empirical findings (do not re-litigate without new data)

1. **Zigzag** (signed-residual → small-unsigned remap inside the delta kernel)
   helps 16-bit data clearly, but showed no gain on uint32. Reasons: (a) on
   uint32 the multiple identical sign-extension byte planes get LZ-deduped by
   zstd anyway; (b) much of the uint32 corpus is piecewise-constant (residuals
   exactly 0, no sign bytes to clean); (c) zigzag only pays when a
   position-sensitive stage (shuffle) follows the delta.
2. **uint32 corpus has three clusters** (from analysis stats):
   piecewise-constant (runs > 0.99 — IDs/masks), structured (entropy 2–6),
   and noisy-limited-range (~24 live bits, top byte ~constant, entropy 6–7.4).
   In **every** chunk, XOR residuals were far sparser than arithmetic ones
   (xor_mean 1.7–11 bits vs delta needing 22–28 bits) → values jump between
   widely-spaced levels sharing most bits → `xordelta` is the right family.
3. **Full-width subtractive delta fails on noisy floats due to borrow
   propagation**: low-mantissa Monte-Carlo noise injects carries into the
   clean exponent/high-mantissa bytes. f32 (2–3 noise bytes) always lost;
   f16 (≤1 noise byte) lost only on noisy channels. Fix: do the delta
   **per byte lane after shuffle** (= our `bytedelta`), which confines
   carries; lane-delta on a pure-noise lane is entropy-neutral
   (uniform − uniform mod 256 = uniform). This is why
   `fmap + shuffle + bytedelta` works.
4. **`fmap`** = monotonic IEEE-754 order map (negative → flip all bits,
   positive → set sign bit; exact inverse in backward). Bijection over all
   bit patterns → NaN/Inf/−0.0/denormals round-trip bit-exactly. Makes
   integer ordering match numeric ordering so deltas behave across sign and
   magnitude changes. Runs FIRST in the float pipeline. typesize 2/4/8;
   rejects 1.
5. **`bytedelta` semantics**: per-lane byte delta over **already-shuffled**
   data (`stream_len = length / typesize`, lane reset at `tid % stream_len == 0`).
   It does NOT include the transpose — must be preceded by `shuffle` in the
   forward list. No zigzag needed after it (nothing position-sensitive
   follows; zstd's entropy stage doesn't care which symbols are frequent).
6. **Second-order delta never beat first-order** on this corpus
   (noise amplification: each delta order ~doubles noise variance).
7. **Statistical filter selection was tried and removed** (didn't pay off in
   practice). Artifacts kept for reference: `analysis_kernels.cu` /
   `analysis.h` (single-pass stats kernel: SAD d1/d2, xor popcount, run
   count, 256-bin byte histogram) and `select.h` (pipeline pruning).
   Known flaw if revisited: the Laplace `bits_d1` estimate uses the **mean**
   |residual| and is wildly inflated by rare huge spikes (piecewise-constant
   data showed bits_d1 ≈ 16–22 despite 99.99% zero residuals). Fix would be
   accumulating Σ bit_length(zigzag(r)) via `__clz` instead of Σ|r|.
8. Diagnostic logging from multiple threads raced (interleaved lines) —
   buffer one record per line before writing.
9. **2D locality: horizontal per-row delta reset is a (small) uniform
   regression; vertical "up" prediction is the right lever.** Measured A/B
   (Blue_Lagoon + MonzaSP1, zstd_gpu, identical binaries via the
   `COMPRESSED_DISABLE_2D` env toggle): resetting the horizontal delta/XOR at
   each scanline start lost on **8/8** type×image cases by ~0.01–0.09%. Reason:
   the reset replaces one (spatially meaningless) wrap-around residual per row
   with a **full literal**, and on this corpus the wrap residual is still
   cheaper than an absolute value. So the roadmap's "reset delta at row starts
   = strict improvement" was **wrong on this data**. The next lever tried was
   *vertical* prediction (residual against the pixel directly above): forward
   one-thread-per-element (neighbour `-1` → `-width`, coalesced), backward a
   per-column prefix scan (first row of each chunk a literal so chunks stay
   independently decodable). It was prototyped behind the `row_stride` arg and
   then removed once measured.
   **Result: vertical also LOST, by more than row-reset — 8/8 cases, ~0.7–2.3%
   worse than 1D horizontal** (Blue_Lagoon/MonzaSP1, zstd_gpu; worst = Monza
   float −2.3%, half −1.5/1.7%, uint16/uint32 ~1%). Takeaway: **horizontal
   (left) correlation dominates vertical on this corpus**; predicting from the
   pixel above discards the strongest neighbour. So *neither* simple 2D
   predictor (row-reset, vertical) beats plain 1D horizontal. The only 2D
   approach that can win is one that **combines** left+above and selects
   locally (MED/LOCO-I) — it keeps the dominant horizontal term while
   recovering vertical edges (see finding #10). Consequently **1D horizontal is
   the default, and the row-reset and vertical kernels were removed** (only the
   `row_stride` filter-ABI arg remains, currently unused). Don't re-run
   row-reset or vertical-only expecting a win without new data/corpus.

10. **MED / LOCO-I (JPEG-LS) predictor: implemented, measured, not a win on
    this corpus.** Added as a real filter (`med` plugin, value-domain 2D median
    predictor over left/above/above-left, zigzag on 16-bit, parallel forward,
    anti-diagonal wavefront backward). A/B (zstd_gpu) replacing the per-type
    predictor: **uint16** = a wash (MonzaSP1 **+0.54%**, Blue_Lagoon −0.12%) —
    the clean 2D-vs-1D-delta test, MED only helps on structured/CGI content;
    **uint32** = clear **loss** (MonzaSP1 **−3.76%**) because subtractive
    residuals fight uint32's XOR-friendly structure (confirms finding #2 — keep
    `xordelta`). half/float untouched (control held byte-exact). Net: even the
    "good" 2D predictor gives at best marginal ratio here, and its wavefront
    backward makes decompression far costlier (≈ one kernel launch per
    anti-diagonal). **Defaults are delta/xordelta and the `med` plugin was
    removed** (not worth the maintenance for no benefit); this finding is kept
    so it isn't re-attempted blindly. Overall takeaway across findings #9–#10:
    **2D spatial prediction (row-reset, vertical, MED) does not beat 1D
    horizontal on this corpus** — the ratio headroom is elsewhere (roadmap #1
    cascaded-for-IDs, #2 bitshuffle, #3 cross-channel decorrelation). If MED is
    revisited, it's most promising on structured integer AOVs, and would need a
    cooperative-groups single-launch wavefront backward before the decode cost
    is acceptable.

## Known issues / tech debt

- **bytedelta exports `int` overflow: FIXED.** The delta-family kernels now use
  `size_t` throughout for lane/length/position math.
- **bytedelta tail bytes**: forward processes all `length` bytes, backward
  reconstructs only `typesize * stream_len` — bytes beyond that (when
  `length % typesize != 0`) are never inverted. `fit_block_size` should
  prevent this; add an explicit `length % typesize != 0 → cudaErrorInvalidValue`
  guard in both exports.
- **Backward-scan occupancy**: `bytedelta_backward` launches `typesize` blocks
  (2–8 total) and typed `delta_backward` launches **one** block for the whole
  buffer. On a 28-SM device this bottlenecks decompression. Fixes (increasing
  effort): restart deltas at nvcomp block boundaries (independently decodable;
  format bump) or a decoupled-lookback multi-block scan per lane.
- Add a round-trip unit test pushing ±0.0, ±Inf, multiple NaN payloads,
  denormals, FLT_MAX/MIN through the full `fmap + shuffle + bytedelta` chain
  (memcmp). Failure mode would be silent corruption.

## Roadmap (priority order, ratio-per-effort)

1. **Route ID-like channels to the Cascaded codec** (zero new code — the
   `compression_options` variant already includes it). Cascaded
   (RLE+delta+bitpack) beats zstd on piecewise-constant uint32 (IDs, masks,
   coverage) and is much faster. Needs per-channel codec choice, e.g. by AOV
   name.
2. **bitshuffle filter** (drop-in, same plugin signature): bit-level
   transpose; isolates live/dead bits that byteshuffle mixes within boundary
   bytes; effectively free fpzip-style sign/exponent/mantissa separation for
   f16. Benchmark vs `shuffle` inside the float pipeline.
3. **Cross-channel decorrelation** (preprocessing stage before per-channel
   split): reversible RCT (lossless JPEG 2000) or YCoCg-R for RGB; simple
   inter-channel deltas for normals. Biggest win for RGB beauty / natural
   images; doesn't disturb tuned per-channel filters.
4. **2D prediction** (the big format project): scanline-stride (`row_stride`)
   is now plumbed through the filter API (see finding #9). Step 1 (reset delta
   at row starts) was tried and is a **regression** — removed. Step 2
   (up-delta / vertical) was tried and **lost** (finding #9). Step 3 (MED /
   LOCO-I, the `med` plugin) was implemented and tried and is **also not a win**
   here (finding #10: uint16 wash, uint32 loss). All three 2D tiers are now
   exhausted on this corpus — **1D horizontal is the default**. Ratio headroom
   is in #1/#2/#3 below, not 2D prediction. `row_stride` plumbing + the `med`
   filter are kept for opt-in / future structured-AOV use.
5. **If static pipelines stop sufficing**: per-AOV-name pipeline/codec table
   first; then sampled micro-trial (filter + compress ONE 64 KB block through
   the 2 candidate pipelines, pick the winner for the chunk — <1% overhead,
   measures the real objective including LZ effects, unlike the removed
   statistical estimators).

## Conventions for future changes

- New filters follow the plugin pattern: `<name>_kernels.cu` exporting
  `run_<name>_forward/backward` with the shared signature; hook header
  cloned from `bytedelta.h`; CMake target `compressed_<name>_plugin`
  (name must match the hook header's `s_plugin_name`).
- pip/format changes that alter what backward must do require versioning —
  `compressed_chunk` already stores the filter list, so *which* filters ran
  is covered; *how* a filter behaves (e.g. block-boundary resets) is not.
- Benchmark protocol: compare real compressed sizes per channel category
  (smooth AOVs / noisy beauty / IDs / natural), not entropy proxies; dedupe
  repeated chunks before aggregating; one variable at a time.