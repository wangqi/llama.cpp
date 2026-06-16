# llama.cpp Upgrade: b9553 → b9663

**Date:** 2026-06-16
**Commits in range:** 113 upstream commits merged

---

## New Features

### New Vision Models
- None. All 33 vision/audio encoders under `tools/mtmd/models/*.cpp` are unchanged and already wired into `build-xcframework-ios.sh`.

### New Text Model Architectures
| Model | PR | Notes |
|-------|-----|-------|
| Cohere2 MoE (North Code) | #24260, #24601 | New `LLM_ARCH_COHERE2MOE`; dedicated North Code chat parser (#24615); also feeds the TINY_AYA vocab path |
| EAGLE3 speculative decoding | #18039 | New `LLM_ARCH_EAGLE3` draft-model arch for speculative decoding |

### Multimodal (mtmd) — Video and Batching
- **Video input support** (#24269): mtmd can now ingest video; refactored video subprocess handling (#24316).
- **Lazy bitmap API**: `mtmd_bitmap_init_lazy` reads large video frame-by-frame via a callback without loading the whole file into memory; tracks a whole video under one ID.
- **Batch encoding API** (#24384): new `mtmd_batch_*` functions plus `batch_max_tokens` context param; `build_vit` batching (#24352) for faster multi-image/frame encode.
- **Post-decode callback** (#24645) and `mtmd_get_marker()` accessor added.
- Fix: mtmd miscounting `n_tokens` (#24656).

### Audio / GGML Ops
- New `GGML_OP_COL2IM_1D` op + `ggml_col2im_1d()` (#24206) for 1D conv-transpose in audio models.
- Metal: fixed im2col 1D case for audio models (#24220); added `repeat` for bf16 (#24638).

### Graph / Correctness
- Granite Speech: apply embedding scale when deepstack is not used (#24357).
- plamo2: fix attention key/value length regression (#24317).
- Guard iswa `kq_mask` on its own buffer (#24294).
- LFM2: fix tool-call parsing double-escaping (#24667) and json_schema being ignored (#24377).

### Other
- HEIC/HEIF image input support in the bundled UI (#24137) — not used by our app.
- EXIF JPEG orientation handling (#24196).
- ggml bumped 0.14.0 → 0.15.1.

---

## API Changes

### `include/llama.h`
- No changes in this range.

### `ggml/include/ggml.h`
- **Added**: `GGML_OP_COL2IM_1D` enum + `ggml_col2im_1d(ctx, a, s0, oc, p0)`.
- **Changed**: `ggml_gated_delta_net(...)` gains a trailing `int64_t K` argument (state-snapshot count). Internal to llama.cpp graph build; no app-level call site.

### `tools/mtmd/mtmd.h`
- **Added**: `mtmd_batch` type + `mtmd_batch_init/free/add_chunk/encode/get_output_embd`; `batch_max_tokens` context param; `mtmd_bitmap_init_lazy` + `mtmd_bitmap_lazy_callback`; `mtmd_get_marker()`; `mtmd_encode_chunk()`.
- **Deprecated**: `mtmd_encode()` → use `mtmd_encode_chunk()`. Still compiles; our wrapper (`LLaMa_MModal.swift`) uses the higher-level `mtmd_tokenize` / `mtmd_helper_eval_chunks` path, so no change required.

### `tools/mtmd/clip.h` (internal)
- `clip_image_encode` / `clip_image_batch_encode` now take `std::vector<float> &` instead of `float *`.
- `clip_model_n_batch_max` replaced by `clip_support_batch()` + `clip_model_n_temporal_merge()`.
- All internal to the from-source clip build; not part of our Swift surface.

### State Save/Load Behavioral Changes
- None. No session-cache invalidation required.

---

## Risk Assessment

### LOW: mtmd/clip internal signature churn
Several internal clip/mtmd signatures changed, but all are compiled from source as one unit and our Swift wrapper only touches the stable `mtmd_tokenize` / eval-chunks path. No action required.

### LOW: `ggml_gated_delta_net` K param
Additive arg consumed inside llama.cpp graph build for gated-delta models (e.g. Qwen3 Next). No app call site.

### LOW: New arches Cohere2 MoE + EAGLE3
Additive arch registration. Run-on-demand only when such a GGUF is loaded.

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| clip-models copy | n/a (in-tree CMake) | explicit `cp -fp` + CMakeLists sed patch |

**No structural changes** — no new `tools/mtmd/models/*.cpp` files, so the copy block and CMake sed patch are unchanged.

---

## Action Items

1. **REQUIRED**: rebuild the xcframework — `thirdparty/llama.cpp/build-xcframework-ios.sh`.
2. **Recommended**: smoke-test one vision model (e.g. Qwen3 VL) and one audio model after rebuild to confirm the col2im_1d / im2col-1D and clip signature changes didn't regress.
3. No session-cache invalidation needed.
