# llama.cpp Upgrade: b10333 → b10724

**Date:** 2026-08-31
**Commits in range:** 391 upstream commits merged (`tag-b10333..tag-b10724`, excluding merges)

---

## New Features

### New Vision / Audio Encoders

Five new `.cpp` files appeared in `tools/mtmd/models/` and are now copied by
`build-xcframework-ios.sh`:

| File | PR | Architecture note |
|------|-----|-------------------|
| `dots3note.cpp` | #27524 | dots3-note vision **and** audio front-end (two graphs in one TU) |
| `muse-glimmer.cpp` | #26841 | Muse Glimmer vision tower |
| `pockettts-gen.cpp` | #26871 | Pocket-TTS generation head (`CLIP_MODALITY_GEN_AUDIO`), flow-matching decoder |
| `pockettts-seanet.cpp` | #26871 | Pocket-TTS SEANet / Mimi decoder (continuous features → PCM) |
| `pockettts-spkenc.cpp` | #26871 | Pocket-TTS speaker encoder (voice cloning conditioning) |

Existing encoders and the image pipeline also changed:

- **Pillow-accurate resize for every model** (#27594) — `resize_algo` is now selected per
  model instead of one global default, and the resampling matches PIL bit-for-bit. This
  shifts OCR / vision preprocessing output slightly for *all* existing vision models.
- **`mtmd_bitmap_set_mergeable`** (#27348) — `[QWEN_VIDEO]` temporal frame merging became
  opt-in. See the MEDIUM risk item below; the Swift wrapper needed a fix.
- **sha256 input hashing** (#27274) — `mtmd-helper.cpp` now includes `"hash/hash.h"` and
  hashes media inputs with SHA-256 instead of the previous scheme. This pulls in the new
  `vendor/hash/` sources; see the HIGH risk item below.
- **DeepSeek-OCR SAM `ggml_conv_2d` keeps im2col in F32** (#26727) — accuracy fix for OCR.
- **Granite 4 Vision image sequence assembly** fixed (#26653) and `preprocessor_granite`
  hardened against malformed metadata (#27235).
- **LFM2 image tiling threshold** fixed (#27057); thumbnails skipped for non-tiled LFM2
  images (#27246).
- **`--mmproj-device`** (#23255) — `mtmd_context_params` / `clip_context_params` gained a
  `device` field so the projector can run on a different backend than the text model.
- **webp decoding via ffmpeg** (#27520) and a moov-atom-at-end-of-file video fix (#27596).
  Both live behind `MTMD_VIDEO`, which our build leaves OFF (`LLAMA_SUBPROCESS=OFF`), so
  neither reaches the app.
- **mmproj quantization restored** (#26818) — a new quant-only `LLM_ARCH_CLIP` stub
  (`src/models/clip.cpp`) lets `llama-quantize` open mmproj GGUFs again.

### New Text Model Architectures

**Edge column**: the app can only realistically run models at or below roughly 4B total
parameters on device, since every weight has to be resident. For MoE models that means *total*
params, not active params. Sizes are the `LLM_TYPE` values declared in `src/models/<arch>.cpp`.

| Model | PR | Size (total / active) | Edge (<= 4B) | Notes |
|-------|-----|----------------------|--------------|-------|
| **Granite-SWA** | #25505 | **3B** | **YES** | `GraniteSWAForCausalLM` / `GraniteMoeSWAForCausalLM`, sliding-window attention |
| **Granite-Switch** | #25107 | **3B** / 8B / 30B | **YES (3B)** | New switch-routing Granite family; the 3B is the edge-viable member |
| **Pocket-TTS** | #26871 | **109M / 335M** | **YES** | Tiny TTS backbone; pairs with the three `pockettts-*` encoders above |
| **Nanbeige 4.2 3B** | #27730 | **3B** | **YES** | Extends the b10333 Nanbeige 4.2 support down to the published 3B checkpoint |
| BailingMoE3 | #26608 | 7.9B-A1.3B / 124B-A5.1B | no | Plus DSpark sidecar support (#27508) |
| Muse Glimmer | #26841 | 30B | no | Tool-call detection after EOM fixed in #26879 |
| Qwen3.8-Flash-Next (`qwen4exp`) | #27742 | A3B | no | Needs top-k radix select for k >= 1024 (#28032 / #28073) |
| dots3-note | #27060 | 288B-A19B | no | Vision + audio via `dots3note.cpp` (#27524) |
| Kimi-K3 | #26185 | 2.8T-A50B | no | Text model only |
| MiniMax-01 / MiniMax-M1 | #27018 | 456B | no | `MiniMaxText01ForCausalLM`, `MiniMaxM1ForCausalLM` |
| GLM-4.5-Air MTP | #26534 | 106B-A12B | no | Multi-token-prediction draft head |
| Nemotron MTP / Nemotron 3.5 DSpark | #26725, #27804, #26905 | frontier scale | no | MTP + DSpark + DFlash paths |
| LFM2 DSpark | #27383 | 350M-2.6B | (base yes) | DSpark sidecar for the already-supported LFM2 family |

### Lazy Tensor Reading

`llama_model_params` gained `lazy_mode` (#27794, refined in #27837, CLI flag renamed to
`--lazy-mode` in #27969). Rows of arch-marked tensors are read on demand instead of up front.
The default is `LLAMA_LAZY_MODE_AUTO`, which only engages for marked tensors **larger than
4 GiB** and requires mmap — no on-device model comes close, so this is inert for the app.

### Metal

- **Per-op source split + parallel compile** (#26561) — the monolithic
  `ggml/src/ggml-metal/ggml-metal.metal` was deleted and replaced by ~20 files under
  `ggml/src/ggml-metal/kernels/`. `GGML_METAL_EMBED_LIBRARY=ON` handles the split upstream
  (it emits one embedded metallib per kernel source), so our build script needs no change.
- **Flash-attention vec tuning tables** landed for essentially every Apple GPU the app runs
  on: M1 (#28078), M1 Max (#27932), M2 (#27940, #28017), M3 Pro (#27963), M3 Max / M5 /
  M5 Pro (#27863), M3 Ultra (#27999), M4 (#27875), M4 Pro (#27824, #27915), plus the
  per-device `(Q, NE)` selection mechanism itself (#26570). This is the single largest
  user-visible speed change in the range for Apple Silicon.
- **Quantized KV cache is dequantized to F16 before flash attention** (#27390), and only for
  large batches (#27438) — fixes correctness with quantized KV plus FA.
- **Memory leaks from missing autoreleasepools** fixed (#27758) — relevant on device, where
  the app holds a context across many turns.
- **Null-pipeline crash for F16 `src1` in `mul_mat` / `mul_mat_id`** fixed (#25648) and a
  **null-check on buffer allocation to avoid an OOM crash** (#25371).
- **Shared-memory padding assert** added (#27951); **K extent clamped** in the tensor-API
  mat-mat kernel when K is not a multiple of 32 (#27450).
- **TQ2_0 support** (#26980), **packed q8_0 dequant** (#27370), **top-k radix select**
  (#28073), **chunked SSD MMA for Mamba-2 prefill** (#26647).

### Stability and Security

- **GGUF loader hardened against malformed tensor dims and metadata types** (#25596).
- **GGUF array type checked before reading** (#27075).
- **LoRA tensor data bounds-checked against file size** (#27056).
- **wavtokenizer-dec posnet/convnext `block_count` bounded against `n_layer_all`** (#26892).
- **Integer tokenizer scores** now accepted by the vocab loader (#27260).
- **Context-shift crash with an unquantized K cache** fixed — the Hadamard matrix is only
  copied to `k_rot` when that tensor has a buffer assigned (#27967).
- **Quadratic cost in Jinja `gather_string_parts`** fixed (#27034) — a real latency win on
  long chat templates.
- **Chat-template fixes**: Qwen3-coder workarounds scoped (#27679), bare-function parsing
  tightened for Qwen (#26793), LFM2 tool-call arg-name ambiguity (#26960), Laguna-S-2.1
  aligned to HuggingFace (#26232), `reasoning_effort` passed to templates.

---

## API Changes

### `include/llama.h`

- **Added**: `enum llama_lazy_mode` (`OFF` / `AUTO` / `ON`) and `llama_model_params.lazy_mode`.
  Default `AUTO`. Struct layout changed — anything constructing `llama_model_params` by
  designated initializer rather than `llama_model_default_params()` must be revisited.
  The Swift wrapper uses `llama_model_default_params()`, so it is unaffected.
- **Added**: `LLAMA_LOAD_MODE_AUTO = -1` to `enum llama_load_mode`, and it is now the
  effective default for CLI tools (#26081) — auto-detect, avoiding mmap on iGPUs.
  `LLaMa.swift` always sets `load_mode` explicitly via `Self.loadMode(...)`, so the new
  auto-detection never runs for us.
- **Added**: `llama_context_params.n_outputs_max_per_seq` (0 = fall back to `n_outputs_max`).
- **Added**: `llama_version()`.
- **Added**: `llama_model_quantize_params.max_buf_size` (0 = default 8 GiB) — caps working
  memory during quantization (#27795, #22877).
- **Added**: `llama_sampler_copy(src, dst)`.
- **Changed**: `llama_sampler_i.backend_init` now takes a third argument
  `uint32_t n_outputs_max_per_seq`; `backend_reset` and `copy_state` were added to the vtable
  (#25532, multi-output backend sampling). **Only affects code implementing a custom
  `llama_sampler_i`.** The wrapper only calls the built-in `llama_sampler_init_*`
  constructors, so nothing to do.
- **Changed**: `llama_state_seq_load_file` now reports only the token count when `tokens_out`
  is NULL.

### `ggml/include/ggml.h`

- **Added**: `GGML_GLU_OP_SWIGLU_CLAMP` and `ggml_swiglu_clamp()`.
- **Added**: `ggml_rope_set_offset()` (+ Metal support, #27120) — used by mtmd (#27521).
- **Changed**: `ggml_clamp()` is no longer in-place; `ggml_clamp_inplace()` is the new
  in-place variant. Internal to ggml consumers; nothing in our code calls it.
- **Changed**: the `A, B, C, ids` op now takes a trailing `int64_t K`.
- **Version**: ggml `0.19.0` → `0.22.0`.

### `tools/mtmd/mtmd.h`

- **Added**: `mtmd_context_params.device` (`ggml_backend_dev_t`) — new field in the middle of
  the struct. Safe for us because `LLaMa_MModal.swift` builds params with
  `mtmd_context_params_default()`.
- **Added**: `mtmd_bitmap_set_mergeable(bitmap, bool)` — see the behavioural change below.
- **Added**: `mtmd_input_chunk_get_placeholder()`, `mtmd_gen_inp_default()`.
- **Added**: `MTMD_GEN_AUDIO_TYPE_POCKETTTS`; `mtmd_gen_audio_info.model_variant`;
  `mtmd_gen_inp` gained `seed`, `temp`, `feats`, `n_feats`; `mtmd_gen_out` gained `feats`,
  `n_feats`, `is_eos`. Not used by the app (no TTS-through-mtmd path yet).

### `tools/mtmd/mtmd-helper.h`

- **Added**: `struct mtmd_helper_video_init_params` (`fps_target`, `ffmpeg_bin_dir`,
  `timestamp_interval_ms`) and `struct mtmd_helper_init_opt` wrapping it, plus
  `mtmd_helper_video_init_params_default()` and `mtmd_helper_init_opt_default()`.
- **BREAKING**: `mtmd_helper_bitmap_init_from_file()` and `mtmd_helper_bitmap_init_from_buf()`
  both gained a trailing `struct mtmd_helper_init_opt opt` parameter (#27520, the webp/ffmpeg
  decode path). Every existing call site fails to compile until the new argument is supplied.
  `LLaMa_MModal.createBitmapUsingHelperAPI()` passes `mtmd_helper_init_opt_default()` — our build
  has `MTMD_VIDEO` off via `LLAMA_SUBPROCESS=OFF`, and that call site is image/audio only, so
  none of the video fields apply.
- **Behavioural**: bitmaps built by these helpers now carry a **SHA-256 hex string** as their id
  instead of an FNV hash (#27274). Anything persisting or comparing bitmap ids across versions
  sees different values.
- **Unchanged**: `mtmd_helper_eval_chunks()` — verified by diff; the four call sites in
  `LLaMa_MModal.swift` needed no edit.

### `tools/mtmd/clip.h`

- **Added**: `clip_context_params.device`; `clip_encode_params` gained `out_feats`, `seed`,
  `temp`, `out_is_eos`, `feats`.

### New Internal Header

`tools/mtmd/mtmd-internal.h` was split out of `mtmd.cpp` (#27348). `mtmd.cpp` includes it
unconditionally, so it is mandatory for the xcframework build even though it adds no
translation unit.

### State Save/Load Behavioral Changes

- `LLAMA_SESSION_VERSION` 9 → 10
- `LLAMA_STATE_SEQ_VERSION` 2 → 3

Any existing `llama_state_save_file` output is now rejected on load. **No action needed**:
`ModelAndContextParams.save_load_state` defaults to `false` and `ChatConfig.swift` keeps the
assignment commented out, so the app has never written a session file.

---

## Risk Assessment

### HIGH: `mtmd-helper.cpp` now needs the vendored hash sources — link failure without a fix

**Problem:** #27274 made `mtmd-helper.cpp` include `"hash/hash.h"` unconditionally. Upstream
satisfies this with a new `vendor::hash` **static library** wired up by the new
`vendor/CMakeLists.txt` (`add_subdirectory(vendor)` at the project root). Our xcframework
build does not use that: `combine_static_libraries()` merges exactly
`libllama.a`, `libggml.a`, `libggml-base.a`, `libggml-cpu.a`, `libggml-metal.a` and
`libggml-blas.a`. A separately-built `libvendor-hash.a` would never reach the app, so the
link would fail with an undefined `hash_sha256_hex` — or, worse, configure would fail first
because `src/` has no `hash/` directory on the include path at all.

**Fix applied:** mirror the existing `src/stb` / `src/miniaudio` pattern.
`copy_mtmd_files()` now copies `vendor/hash/.` into `src/hash/` (dropping its
`CMakeLists.txt`), and `src/CMakeLists.txt` compiles `hash/hash.cpp`,
`hash/sha256/sha256.c`, `hash/sha1/sha1.c` and `hash/xxhash/xxhash.c` straight into the
`llama` target. Two details matter:

- `sha1.h` wraps its API in `namespace vendor_hash`, so `sha1.c` only compiles as C++ —
  `set_source_files_properties(hash/sha1/sha1.c PROPERTIES LANGUAGE CXX)`, matching
  `vendor/hash/CMakeLists.txt`.
- `sha256.c` includes `"rotate-bits/rotate-bits.h"` relative to the hash root, so
  `target_include_directories(llama PRIVATE hash)` was added.

`build-xcframework-ios.sh` also fails fast with an explicit message if `src/CMakeLists.txt`
is missing the `hash/hash.cpp` entry, rather than dying deep inside the compile.

**Verified:** a native macOS configure + `cmake --build --target llama` completes; `nm` shows
`hash_sha256_hex`, `sha256_hash`, `vendor_hash::SHA1Init` and the `XXH*` symbols all defined
inside `libllama.a`, and the five new `clip_graph_*` vtables are present.

### HIGH: `mtmd-internal.h` must be copied or the build fails

**Problem:** `mtmd.cpp` includes `"mtmd-internal.h"` (#27348). The build script flattens the
mtmd sources into `src/`, so the header has to be copied alongside them.

**Fix applied:** `cp -fp "tools/mtmd/mtmd-internal.h" src/` in `copy_mtmd_files()`, with a
matching `rm -f src/mtmd-internal.h` in the cleanup block and a `.gitignore` entry.

### MEDIUM: video frame merging silently stopped working

**Problem:** before b10724, `mtmd_bitmap::can_merge_with()` merged **any** two adjacent
same-size image bitmaps:

```cpp
return !is_audio && !other.is_audio && nx == other.nx && ny == other.ny;
```

#27348 added a `mergeable` flag defaulting to `false` and made it a precondition on both
sides:

```cpp
return mergeable && other.mergeable && !is_audio && !other.is_audio && nx == other.nx && ny == other.ny;
```

`LLaMa_MModal.createBitmapsFromVideo()` decodes N evenly-sampled frames itself (our
xcframework has no ffmpeg) and hands them to `mtmd_tokenize` relying on that automatic
merge. After the upgrade the frames would be treated as N independent images: roughly double
the vision tokens for a video on Qwen-VL-family models, and no temporal pairing.

**Fix applied:** `createBitmapsFromVideo()` now calls `mtmd_bitmap_set_mergeable(bitmap, true)`
on each frame. All frames come from one video through the same
`AVAssetImageGenerator.maximumSize`, so they share identical `nx`/`ny` and merging is exactly
the pre-b10724 behaviour. The stale comment in `make_media_embed()` was updated to match.

Note the flip side is a genuine upstream fix: two *unrelated* same-size still images passed
together used to be merged as if they were video frames. They no longer are.

### MEDIUM: vision preprocessing output shifts for every model

**Problem:** #27594 switched image resizing to a Pillow-accurate implementation and corrected
`resize_algo` per model. Vision and OCR output will differ slightly from b10333 for the same
input, even on unchanged models.

**Mitigation:** no code change is possible or wanted — upstream is now matching the reference
Python preprocessing, which should be a net accuracy win. Worth a spot-check of Doc Scanner
output on a couple of known pages after the xcframework is rebuilt.

### LOW: Metal kernel sources split into ~20 files

`ggml-metal.metal` was deleted in favour of `ggml/src/ggml-metal/kernels/*.metal` (#26561).
`GGML_METAL_EMBED_LIBRARY=ON` handles this upstream and the build script never referenced the
old path. Expect a different (parallel, generally faster) shader compile step.

### LOW: session / state file version bump

`LLAMA_SESSION_VERSION` 9 → 10 and `LLAMA_STATE_SEQ_VERSION` 2 → 3. The app never writes
session files (`save_load_state` is `false` everywhere), so there is nothing to invalidate.

### LOW: `llama_load_mode` auto default

`LLAMA_LOAD_MODE_AUTO` became the default for upstream tools. `LLaMa.swift` always sets
`load_mode` explicitly from the app's mmap / mlock / direct-I/O preferences, so behaviour is
unchanged.

### LOW: `lazy_mode` defaults to AUTO

Only engages for arch-marked tensors over 4 GiB and requires mmap. Inert on device.

### LOW: root CMake now defines `LLAMA_VERSION_BASE` / `LLAMA_VERSION_MAJOR`

`LLAMA_INSTALL_VERSION` was replaced by an explicit `LLAMA_VERSION*` block at the project
root. `src/CMakeLists.txt` reads `LLAMA_VERSION_BASE` / `LLAMA_VERSION_MAJOR`, both of which
the new block defines. Configure succeeds (verified: `llama.cpp version: 0.3.0-dev`).

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| mtmd sources | built as a separate `mtmd` library from `tools/mtmd/` | copied into `src/` and compiled into `libllama` |
| Vendored deps | `vendor::hash` / `vendor::stb` / `vendor::miniaudio` targets | copies into `src/hash`, `src/stb`, `src/miniaudio` |
| `LLAMA_SUBPROCESS` | ON | **OFF** (Catalyst SDK marks `posix_spawn_file_actions_addchdir_np` unavailable; also keeps `MTMD_VIDEO`/ffmpeg off) |
| Framework assembly | CMake install | manual `libtool` merge of 6 named `.a` files |

**Structural changes needed this cycle:** the vendored-hash copy and `mtmd-internal.h` copy
described above, plus the five new encoders. Everything else was absorbed without change.

---

## Action Items

1. **DONE (required)**: copy `vendor/hash/` into `src/hash/` and compile it into the `llama`
   target — without this the build does not link.
2. **DONE (required)**: copy `tools/mtmd/mtmd-internal.h` into `src/`.
3. **DONE (required)**: copy the five new encoders into `src/clip-models/`.
4. **DONE (required)**: call `mtmd_bitmap_set_mergeable(true)` on video frames in
   `LLaMa_MModal.createBitmapsFromVideo()` to restore temporal merging.
5. **TODO**: rebuild the xcframework (`./build-xcframework-ios.sh`) and confirm the iOS
   device / simulator / macOS / Catalyst slices all link.
6. **Recommended**: spot-check Doc Scanner OCR output against a known page — image resizing
   changed for every vision model (#27594).
7. **Recommended**: spot-check a video prompt on a Qwen-VL-family model to confirm frame
   merging is back (the debug log line is `merging 2 frames at part index ...`).
