# llama.cpp Upgrade: b10091 → b10333

**Date:** 2026-08-10
**Commits in range:** 234 upstream commits merged (`tag-b10091..tag-b10333`, excluding merges)

---

## New Features

### New Vision / Audio Encoders

Five new `.cpp` files appeared in `tools/mtmd/models/` and are now copied by
`build-xcframework-ios.sh`:

| File | PR | Architecture note |
|------|-----|-------------------|
| `minimax-m3.cpp` | #25113 | Vision tower for MiniMax-M3 (MSA sparse-attention backbone) |
| `mimo-audio.cpp` | #26190 | MiMo-V2.5 audio input, RVQ (residual vector quantization) codec front-end |
| `parakeet.cpp` | #22520 | Parakeet ASR encoder used by Nemotron 3 Nano Omni |
| `qwen3tts-gen.cpp` | #26254 | Qwen3-TTS generation head (code → wav, `CLIP_MODALITY_GEN_AUDIO`) |
| `qwen3tts-spkenc.cpp` | #26254 | Qwen3-TTS speaker encoder (voice conditioning) |

Existing encoders also gained capability:

- **GLM-5.2-Vision** (#26126) — new projector wired through `glm4v.cpp`.
- **MiniCPM-V 4.6 downsample** (#25993).
- **DeepSeek-OCR multi-row batching** (#26154) — faster multi-tile OCR.
- **Unlimited-OCR `max_tiles`** fix (#25614) — the converter now writes the real tile cap.
- **`longest_edge` now honours min/max pixels** (#26638) — fixes over/under-sized image resizes.
- **Lanczos resize method** added to the image pipeline (#26341).
- **Empty-audio chunk fix for short inputs** (#26536).
- **Qwen2.5-Omni mmproj conversion regression** fixed (#26262).

### New Text Model Architectures

**Edge column**: the app can only realistically run models at or below roughly 4B total
parameters on device, since every weight has to be resident. For MoE models that means *total*
params, not active params — a 30B-A3B model is only 3B active but still needs all 30B in memory.
Sizes are the `LLM_TYPE` values declared in `src/models/<arch>.cpp`.

| Model | PR | Size (total / active) | Edge (<= 4B) | Notes |
|-------|-----|----------------------|--------------|-------|
| **Nanbeige4.2** | #25994 | **3B** | **YES** | Only edge-viable architecture in this range. Looped Transformer: reuses layers to add capacity without adding params, which is why `nanbeige.cpp` declares `LLM_TYPE_UNKNOWN` instead of a layer-count lookup. GGUF quants already published. |
| MiniMax-M3 | #24908 | 428B-A23B | no | MSA (MiniMax Sparse Attention); MSA moved to its own memory impl in #26338 |
| Laguna-S-2.1 | #26233 | 118B-A8B | no | New `LLM_TYPE` for the Poolside Laguna family (XS.2 is 30B-A3B, M.1 is 230B-A10B) |
| GLM-5.2 Indexer | #25407 | 744B-A40B | no | Sparse-attention indexer tensors |
| GLM-5.2 (GLM_DSA) NextN/MTP | #25980 | 744B-A40B | no | Multi-token-prediction draft head |
| GLM-4.7-Flash MTP | #24868 | 30B-A3B | no | MTP support, built on the `deepseek2` arch path |
| DeepSeek V4 MTP + DSpark | #25784 | frontier scale | no | Plus DSpark sidecar resolution (#26458) and separate-GGUF conversion (#26452) |
| DeepSeek V3.2 MTP | #26457 | 685B-A37B | no | MTP support |
| Qwen3-Next MTP | #25589 | 80B-A3B | no | MTP support |
| DeepSeek V4 Flash 0731 chat template | #26398 | frontier scale | no | New Jinja template |
| ExaoneMoeForCausalLM spelling | #26660 | 30B-A3B / 235B-A22B | no | Conversion now accepts the alternate arch name |

MTP tensors are now loaded lazily — `llama_model_params` gained `load_mtp`, and both
`llama : load MTP tensors only if they are really used` (#26296) and
`model : load MiMo V2 MTP tensors only if used` (#26412) avoid paying for draft weights that
the app never uses. This is a memory win on device, since the app does not enable speculative
decoding.

### Metal / Apple Silicon

- **NORM / RMS_NORM correctness fix** for row lengths that leave a partial simdgroup (#26708).
  This is the most user-visible fix in the range — affected models produced subtly wrong
  activations on rows whose length is not a multiple of the simdgroup width.
- **Memory unwire fix** when a model is freed without ever running a GPU op (#26082). Directly
  relevant to the app's load → evict path and to failed/aborted loads.
- **DeepSeek V4 hyper-connections** implemented on Metal (#26459).
- **DSv4 Lightning Indexer** Metal kernel (#25893), plus a follow-up that avoids `threadgroup`
  matrix-array instantiation in that kernel (#26646) — reduces threadgroup memory pressure.
- **F16 support for binary ops** (#26465), **`SILU_BACK`** (#25982), **f16 leaky-relu** (#25981),
  **FWHT kernel** (#25924).
- `GGML_METAL_USE_BF16` removed from all build scripts (#26604).

### CPU / ARM64

- **aarch64 HWCAP fallbacks and fp16 variant detection fix** (#25554) — corrects feature probing
  when the OS does not report the expected capability bits.
- `ggml : adjust logic for offloading ops to weight's backend` (#25832).
- `ggml: use dynamic allocation for split graph inputs` (#22789) — removes a fixed cap on graph
  input count.
- New `ggml_build_forward_order()` API (#26649) for building graph order without marking nodes
  for compute.

### Quantization

- **Rotated KV-cache quantization** support (#26180).
- **DeepSeek V4 now enforces matching K and V cache types**, and enables flash attention when the
  V cache is quantized (#25871).
- Endianness conversion for Q1 and TQ2 during conversion (#26618).
- `model-loader : fix quantized reshaped tensor strides` (#26672) and
  `llama : allow reshape of tensors during load` (#26531).

### Robustness / Security

- `gguf-py`: validate `n_dims` and guard against uint64 overflow in the reader (#25401).
- `vocab`: validate default special-token ids (#26506) and plamo2 byte tokens (#26511).
- `common`: fix use-after-free when LoRA adapter loading fails (#25611).
- `llama-context`: sync pending async copies before clearing `embd_seq` (#25676).
- `gguf.cpp`: virtual destructor for `gguf_writer_base` (#25867).
- `common : add subproc.h wrapper, disabled on android/ios` (#26102) — upstream explicitly keeps
  subprocess spawning off for iOS; `LLAMA_SUBPROCESS_DEFAULT` is `OFF` when
  `CMAKE_SYSTEM_NAME STREQUAL "iOS"`, so `MTMD_VIDEO` (ffmpeg) stays disabled in our build.

### Chat / Jinja

- Specialized **Qwen3 parser** (#26252) and **MiniMax-M3 parser** (#26210).
- Tool calls inside thinking blocks enabled for DeepSeek V4 (#26269).
- Cohere2 MoE template enforces the JSON schema for text responses when one is supplied (#26018).
- Reasoning-budget sampler supports multiple end sequences (#25544).
- `suppress_tokens` moved into `common/sampling` and exposed via the vocab API (#26276).

---

## API Changes

### `include/llama.h`

- **Added**: `enum llama_load_mode` (`NONE` / `MMAP` / `MLOCK` / `MMAP_MLOCK` / `DIRECT_IO`) plus
  `llama_load_mode_name()` and `llama_load_mode_from_str()`.
- **Removed from `llama_model_params`**: `bool use_mmap`, `bool use_mlock`, `bool use_direct_io`.
  Replaced by a single `enum llama_load_mode load_mode` field (PR #20834).
- **Added to `llama_model_params`**: `bool load_mtp` — whether to load MTP (draft) layers.
- **Changed**: `llama_sampler_init_penalties()` now takes `int32_t n_vocab` as its **first**
  argument (PR #26520). `penalty_last_n == -1` no longer means "context size"; upstream clamps
  negative values to `0`, which disables the sampler. `penalty_repeat` must be `> 0.0` and
  `penalty_freq` / `penalty_present` must be finite — otherwise the call returns an inert
  `"?penalties"` sampler instead of asserting.
- **Changed**: `llama_sampler_init_dry()` dropped its `int32_t n_ctx_train` parameter.
- **Added**: `llama_vocab_get_suppress_tokens()` — reads `tokenizer.ggml.suppress_tokens`.

### `ggml/include/ggml.h`

- **Added**: `ggml_build_forward_order()` — adds a tensor and its parents to a graph without
  marking them for compute.
- RPC protocol version macros bumped (`RPC_PROTO_MAJOR_VERSION`, `RPC_PROTO_PATCH_VERSION`);
  not used on Apple platforms.

### `ggml/include/gguf.h`

No changes.

### `tools/mtmd/mtmd.h`

- **Added**: `MTMD_INPUT_CHUNK_TYPE_COUNT` sentinel.
- **Added**: `mtmd_input_chunk_save()` / `mtmd_input_chunk_load()` (#26645) — serialize chunk
  *metadata* for KV save/load. Loaded chunks are placeholders and cannot be re-encoded.
- **Added (experimental)**: audio-generation API — `mtmd_gen_audio_type`,
  `mtmd_gen_audio_info`, `mtmd_gen_audio_get_info()`, `mtmd_gen_process_type`,
  `mtmd_gen_inp` / `mtmd_gen_out`. Marked "subject to breaking changes" upstream.
- Nothing was removed.

### `tools/mtmd/clip.h`

- **Added**: `CLIP_MODALITY_GEN_AUDIO` modality and `clip_init_result::ctx_gen_a`.
- **Added**: `clip_encode_params` struct and `clip_encode()` covering both the classic image/audio
  encode path and the new `GEN_CODE` / `GEN_WAV` audio-generation path.

### `tools/mtmd/` file layout

- **New**: `mtmd-helper-common.h` — shared internal header (logger + `decode_embd_batch`).
  `mtmd-helper.cpp` now `#include`s it, so it is **mandatory** to copy.
- **New**: `mtmd-helper-gen.cpp` — carries the `mtmd_helper_gen_audio_*` symbols declared in
  `mtmd-helper.h`. Nothing in the app calls them yet, but the framework should export what its
  public header declares.

### State Save/Load Behavioral Changes

None. No session/state version constant was bumped, and no field was added to or removed from
the serialized context state in this range. **Existing session cache files remain valid** — no
invalidation needed.

`mtmd_input_chunk_save/load` is a *new, separate* facility for persisting multimodal chunk
metadata alongside a KV dump; it does not change the existing `llama_state_*` format.

---

## Risk Assessment

### HIGH: `llama_model_params` load-flag removal breaks the Swift bridge

**Problem:** `thirdparty/llamacpp_swift/Sources/swift/LLaMa.swift` set `model_params.use_mlock`,
`use_mmap`, and `use_direct_io`. All three fields are gone in b10333, so the bridge no longer
compiles.

**Required fix (applied):** added `LLaMa.loadMode(useMMap:useMlock:useDirectIO:)`, which maps the
three `ModelAndContextParams` booleans onto `llama_load_mode`, with direct I/O taking precedence
over mmap as the old `use_direct_io` documentation specified. The LoRA path (which previously
forced `use_mmap = false`) now recomputes `load_mode` with mmap dropped, and the model-load
exception context logs `llama_load_mode_name(...)` instead of `use_mmap`.

### HIGH: sampler signature changes break the Swift bridge

**Problem:** `llama_sampler_init_penalties()` gained a leading `n_vocab` argument and
`llama_sampler_init_dry()` lost `n_ctx_train`. Both are called from
`thirdparty/llamacpp_swift/Sources/swift/GPT_SPM.swift`.

**Required fix (applied):** `init_sampling` passes the already-computed `vocabCount` as the new
first argument to `llama_sampler_init_penalties`, and the now-unused `ctxTrain` local was removed
from the DRY call site.

### HIGH: `load_mode` removed the direct-I/O fallback, silently disabling mmap on Apple

**Problem:** direct I/O is implemented only under `#ifdef __linux__` in `src/llama-mmap.cpp`.
Before b10333 the loader probed `llama_file::has_direct_io()` and, when the platform could not
honour the request, logged `"direct I/O is not available, using mmap"`, reopened the file with
`std::fopen`, and cleared `use_direct_io`. On Apple the app's user-facing **Use Direct I/O**
toggle was therefore a harmless no-op.

The enum removed that probe. `llama-model-loader.cpp` now derives the flags unconditionally:

```cpp
this->use_mmap      = load_mode == LLAMA_LOAD_MODE_MMAP || load_mode == LLAMA_LOAD_MODE_MMAP_MLOCK;
this->use_direct_io = load_mode == LLAMA_LOAD_MODE_DIRECT_IO;
```

So `LLAMA_LOAD_MODE_DIRECT_IO` on iOS/macOS means **no mmap and no direct I/O** — the whole model
is read through buffered stdio into anonymous memory. Higher peak RSS and a slower load, which
directly conflicts with the single-local-model-in-memory constraint.

**Required fix (applied):** `LLaMa.loadMode(...)` ignores `useDirectIO` under `#if canImport(Darwin)`
and logs that it fell back, so the effective behaviour matches pre-b10333. The non-Darwin branch
still honours the flag.

**Follow-up (product decision, not applied):** the `use_direct_io` toggle in
`ModelDetailLocalForSettingsView` and `helper/model_settings_*.json` has never done anything on
Apple and now cannot. It should be removed, or replaced by a single load-mode picker that exposes
the modes that do work here (mmap / mlock / both / none).

### MEDIUM: `penalty_last_n = -1` no longer means "context size"

**Problem:** the app's `repeat_last_n` flows through `ChatConfig` → `ModelItem` → `SpmSamplingParams.penaltyLastN`.
Under the old API, `-1` requested a context-sized penalty window. Upstream now clamps negatives to
`0`, which **disables** the penalties sampler entirely.

**Mitigation:** all 46 entries in `helper/models_test.json` and all 41 in `helper/models_audit.json`
use non-negative `repeat_last_n` (defaults are 512 in `ChatConfig` and 64 in
`ModelSettingsTemplate`), so no shipped configuration is affected. A user who manually typed `-1`
into model settings would silently lose repetition penalty rather than crash — upstream returns an
inert sampler, it does not assert.

### MEDIUM: `mtmd-helper` split into two translation units

**Problem:** `mtmd-helper.cpp` now includes `mtmd-helper-common.h`. Copying only the previously
known mtmd files leaves the build with a missing header.

**Mitigation (applied):** `copy_mtmd_files()` copies `mtmd-helper-common.h` and
`mtmd-helper-gen.cpp`; `src/CMakeLists.txt` lists `mtmd-helper-gen.cpp`, with a grep-guarded sed
fallback in the build script for checkouts where that file has not been updated.

### LOW: `GGML_METAL_USE_BF16` was already a dead CMake variable

The ggml-metal option had been removed upstream before b10091; our script kept passing it, which
only produced a "manually-specified variables were not used" warning. #26604 removed it from
upstream's build scripts, and it is now removed from ours in three places.

### LOW: new `load_mtp` model param defaults to the previous behaviour

`llama_model_default_params()` sets it; the bridge does not touch it. Lazy MTP loading only
reduces memory for architectures that ship draft heads.

### LOW: experimental mtmd audio-generation API

`clip_encode()`, `mtmd_gen_*`, and the two Qwen3-TTS encoders are additive. Nothing in the app
calls them. Upstream flags the API as subject to breaking changes, so do not build on it yet.

### LOW: `MTMD_VIDEO` / subprocess

Upstream added an ffmpeg-backed video path guarded by `MTMD_VIDEO`, which itself requires
`LLAMA_SUBPROCESS`. `CMakeLists.txt` forces `LLAMA_SUBPROCESS_DEFAULT=OFF` for
`CMAKE_SYSTEM_NAME STREQUAL "iOS"`, and the `#ifdef MTMD_VIDEO` guards in `mtmd-helper.cpp` mean
`sheredom/subprocess.h` is never included in our build. No action required.

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS device, iOS simulator, macOS, Mac Catalyst |
| mtmd/clip sources | built as a separate `mtmd` target | copied into `src/` and linked into `libllama` |
| Vision encoders | `tools/mtmd/models/*.cpp` in the mtmd target | copied to `src/clip-models/`, picked up by `file(GLOB)` |
| `LLAMA_OPENSSL` | default | forced `OFF` |
| `LLAMA_BUILD_APP` | default `ON` | forced `OFF` (needs `build-info.h`) |
| `GGML_METAL_USE_BF16` | removed in #26604 | removed here too |

**No structural changes** beyond the file-copy additions listed above.

---

## Action Items

1. **REQUIRED (done)**: copy the five new encoders, `mtmd-helper-common.h`, and
   `mtmd-helper-gen.cpp` in `copy_mtmd_files()`; add `mtmd-helper-gen.cpp` to `src/CMakeLists.txt`.
2. **REQUIRED (done)**: port `LLaMa.swift` to `llama_load_mode` and fix the two sampler call sites
   in `GPT_SPM.swift`.
3. **REQUIRED**: clean rebuild — `rm -rf build-apple build-ios-sim build-ios-device build-macos`
   then `./build-xcframework-ios.sh`. The stale-copy wipe at the top of `copy_mtmd_files()` handles
   `src/`, but the CMake caches still hold the removed `GGML_METAL_USE_BF16` entry.
4. **NOT required**: session cache invalidation. No state format change in this range.
5. **Recommended**: after rebuilding, verify the new encoders linked —
   `nm -gU build-apple/llama.xcframework/ios-arm64/llama.framework/llama | grep -i "minimax\|parakeet\|qwen3tts"`.
6. **Recommended**: smoke-test a GGUF with an odd hidden size to exercise the Metal RMS_NORM
   partial-simdgroup fix (#26708), and load/evict a model without generating to exercise the
   Metal memory-unwire fix (#26082).
