# llama.cpp Upgrade: b9663 → b9754

**Date:** 2026-06-21
**Commits in range:** 90 upstream commits merged

---

## New Features

### New Vision Models
- None. No new `tools/mtmd/models/*.cpp` encoders were added in this range (all 33 existing encoders remain wired into `build-xcframework-ios.sh`).

### New Text Model Architectures
| Model | PR | Notes |
|-------|-----|-------|
| EAGLE3 for Qwen3.5 / Qwen3.6 | #24593 | Speculative-decoding draft support extended to the Qwen3.5/3.6 families |
| Step3.5 / Step3.7 flash MTP3 | #24340 | Multi-token-prediction speculative path for Step3.5/3.7-Flash |
| GLM-DSA optional indexer tensors | #24770 | glm-dsa loads DSA indexer tensors as optional (DeepSeek-style sparse attention) |

### Multimodal (mtmd)
- Model load progress callback added to `mtmd_context_params` (#24865) and surfaced through `clip_context_params`.
- Batching support added for InternVL (#24775) and for mtmd-cli with video tests (#24778).
- Preprocessor refactor with `mtmd_image_preproc_out` (#24736); llava-uhd overview-image handling unified to `ov_img_first` (#24769).
- `mtmd_get_memory_usage` fix (#24867); assorted mtmd bug fixes (#24784); UTF-8 handling fix (#24779).

### Audio / Conv
- CUDA `col2im_1d` op added (#24417) — completes the 1D conv-transpose path for audio models alongside the existing Metal/CPU ops.

### Metal
- BF16 support check in the concat kernel (#24747); F16 and BF16 support added to the concat operator (#24724).
- `rope_back` operator implemented for Metal (#24725).

### Grammar / Jinja
- New PEG-based AC parser for stricter GBNF grammar generation (#24869, #24839).
- json-schema-to-grammar spacing aligned with the parsers (#24835).
- Jinja `call` statement implemented (#24847).

### Server (not used by the embedded engine)
- Large amount of router / model-management / load-progress work (#23976, #24828, #24843, etc.). Not relevant to the in-app static-library usage.

### Third-party library updates
- ggml core bumped 0.15.1 → 0.15.2 (ggml/1548).
- cpp-httplib → 0.48.0 (#24787); BoringSSL → 0.20260616.0 (#24693).

---

## API Changes

### `include/llama.h`
- **Added**: `llama_model_n_layer_nextn(const struct llama_model *)` — number of next-N (MTP) layers, used by speculative decoding. Additive; no removals (remaining hunks are whitespace alignment only).

### `tools/mtmd/mtmd.h`
- **Added**: `mtmd_progress_callback` typedef plus `progress_callback` / `progress_callback_user_data` fields on `mtmd_context_params`. Additive; `mtmd_context_params_default()` zero-inits them, and our wrapper (`LLaMa_MModal.swift:211`) already uses that initializer — no code change required.

### `tools/mtmd/mtmd-helper.h` (BREAKING for our wrapper)
- **Changed**: `mtmd_helper_bitmap_init_from_file` and `mtmd_helper_bitmap_init_from_buf` now return a `struct mtmd_helper_bitmap_wrapper { mtmd_bitmap * bitmap; mtmd_helper_video * video_ctx; }` instead of a raw `mtmd_bitmap *` (PR #24865). **Required fix**: `LLaMa_MModal.swift:402` `createBitmapUsingHelperAPI(mediaPath:)` now reads `.bitmap` off the returned wrapper (video path is handled separately via `createBitmapsFromVideo`, so `video_ctx` is unused there). Applied 2026-06-21.

### `tools/mtmd/clip.h` (internal)
- **Added**: `progress_callback` / `progress_callback_user_data` on `clip_context_params`; overflow `GGML_ASSERT` guard in `clip_image_size::area()` (width/height ≤ 46000).
- **Removed/changed**: several low-level `clip_*` accessors and image init/free/batch helpers removed; encode signatures made `const`. These are internal-to-mtmd symbols; our Swift wrapper calls none of them (verified — only stale cached `.build` headers reference them). No impact.

### State Save/Load Behavioral Changes
- None. No changes to `llama_state_save_file` / `llama_state_load_file` semantics. Existing session cache files remain valid.

---

## Risk Assessment

### MEDIUM: mtmd-helper bitmap-init return type change (RESOLVED)
`mtmd_helper_bitmap_init_from_file` / `_from_buf` now return `mtmd_helper_bitmap_wrapper` instead of a raw `mtmd_bitmap *`. Broke the wrapper compile (iOS + macOS). Fixed in `LLaMa_MModal.swift:402` by reading `.bitmap`. Both targets build clean.

### LOW: mtmd / clip context-params struct growth
Additive fields only; `mtmd_context_params_default()` zero-inits them and our wrapper uses it. No action required.

### LOW: clip.h internal API removals
Removed symbols are not referenced by our wrapper source. No action required.

### LOW: ggml 0.15.1 → 0.15.2 minor bump
Patch-level; no API breakage. No action required.

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| clip-models | n/a (in-tree CMake) | explicit `cp -fp` + CMakeLists sed patch (33 encoders) |

**No structural changes** — no new `tools/mtmd/models/*.cpp` files, so the copy block and CMake sed patch are unchanged.

---

## Action Items

1. **REQUIRED**: rebuild the xcframework — `thirdparty/llama.cpp/build-xcframework-ios.sh`.
2. **Recommended**: smoke-test one text model and one vision (mtmd) model on device after rebuild to confirm the framework loads and inferences correctly.
3. No session-cache invalidation needed.
