# llama.cpp Upgrade: b9279 → b9553

**Date:** 2026-06-07
**Commits in range:** 269 upstream commits merged

---

## New Features

### New Vision / Multimodal Models
- **deepseekocr2.cpp** — DeepSeek-OCR 2 vision encoder (PR #20975). Subclasses the existing DeepSeek-OCR graph; improved OCR preprocessing.
- **granite4-vision.cpp** — IBM Granite 4 Vision encoder + model wiring (PR #23545).
- **exaone4_5.cpp** — EXAONE 4.5 vision encoder (PR #21733); qwen2vl-style patch embedding with GQA attention.
- **gemma4uv.cpp** — Gemma 4 *unified* vision projector (non-causal vision, FPE/pre-norm fixes) (PRs #24077/#24082/#24088).
- **gemma4ua.cpp** — Gemma 4 *unified* audio projector; audio RMS-norm eps + projector embedding-size fixes (PRs #23815/#24091).

### New Text Model Architectures
| Model | PR | Notes |
|-------|-----|-------|
| Gemma4ForCausalLM | #23682 | Text-only Gemma 4 conversion support |
| Gemma4 MTP | #23398 | Multi-token-prediction speculative path for Gemma 4 |
| Mellum | #23966 | New code model architecture |
| EXAONE 4.5 | #21733 | LG AI EXAONE 4.5 implementations |
| DeepSeek V3.2 (DSA) | #23346 | DeepseekV32ForCausalLM with generic DeepSeek Sparse Attention |
| Step3.7-Flash | #23845 | Conversion support |
| talkie-1930-13b | #22596 | New model support |
| Granite multilingual embeddings R2 | #22716 | granite-embedding 97m/311m multilingual r2 |
| qwen3 SSM archs | #24031 | Test/arch support for Qwen3 state-space variants |

### Quantization / Tokenizer
- NVFP4 quantized weights: compressed-tensors NVFP4 conversion (#21095), Mistral3 NVFP4 weight scales (#23629), NVFP4 MTP scale tensors (#23563).
- Parallelized quant LUT init (#23595) — faster model load.
- Tokenizer additions: jina-embeddings-v2-base-zh (#18756), LFM2.5-8B-A1B (#23826), MiniCPM5 (#23384), WPM normalizer lowercase (#23899).

### Metal (Apple Silicon)
- Templated GLU kernels for f16/f32 (#23882).
- Restored im2col implementation for large kernels (#23901).
- Residency-set heartbeat reduced 500ms → 5ms (#24074) — snappier memory residency.
- Added Apple device id reporting (#23566).

---

## API Changes

### `include/llama.h`
- **Added** `llama_context_params.n_outputs_max` — max outputs per ubatch (0 = n_batch). New field; zero-init is safe default.
- **Added** `llama_context_params.ctx_other` — optional source/parent context for sharing results/`llama_memory` between contexts. New field; nullptr default.
- **Deprecated** `llama_set_warmup(...)` — now `DEPRECATED`; user code should do warmup runs manually. Not called by our wrapper.
- **Behavioral** `LLAMA_STATE_SEQ_FLAGS_ON_DEVICE`: getting state for a seq_id with this flag now invalidates all prior on-device states for that seq_id. We do not use the on-device state flag.

### `ggml/include/gguf.h`
- **Added** `gguf_init_from_buffer(...)` (previously commented out) and `gguf_init_from_callback(...)` with `gguf_reader_callback_t` for streamed GGUF parsing (#22341). Additive.

### `ggml/include/ggml.h`
- **Doc-only** `ggml_silu_back` argument comments corrected (a=dy, b=x). No signature change.

### `tools/mtmd/clip.h`
- **Removed** several low-level helpers: `clip_embd_nbytes`, `clip_embd_nbytes_by_img`, `clip_image_u8_get_data`, `clip_build_img_from_pixels`, `clip_get_newline_tensor`, `clip_encode_float_image`, `clip_image_f32_batch_add_mel`. **Verified none are referenced by our Swift wrapper** (we use the high-level `mtmd_*` API). No impact.
- **Added** `clip_model_n_batch_max(...)` and `clip_image_size` helpers (`operator==`, `area()`).

### `tools/mtmd/mtmd.h`
- **Added** placeholder-bitmap support: `mtmd_bitmap_init(..., data=nullptr)` creates an empty bitmap for token counting (#23913). Additive.
- **Added** consecutive-bitmap "frame merge" for qwen-vl video models — handled automatically inside `mtmd_tokenize()` (#21858).

### State Save/Load Behavioral Changes
- No change to host-memory `llama_state_save_file` / `llama_state_load_file` format. Only the on-device-state flag (unused by us) changed invalidation semantics. **No session cache invalidation required.**

---

## Risk Assessment

### LOW: New vision/audio encoder files
5 new `.cpp` files added to `tools/mtmd/models/` and to `copy_mtmd_files()`. The build uses `file(GLOB clip-models/*.cpp)`, so once copied they compile automatically. All five structs are declared in `models.h`. No action beyond the copy block (done).

### LOW: clip.h symbol removal
Removed low-level clip helpers are not referenced by our wrapper (`LLaMa_MModal.swift` uses `mtmd_*`/`mtmd_helper_*`). Verified by grep over `thirdparty/llamacpp_swift/`, `libs/`, `ai/`.

### LOW: llama.h new fields / deprecation
`n_outputs_max` and `ctx_other` are additive (safe zero/nullptr defaults). `llama_set_warmup` deprecation is a compiler warning only; not called by our code.

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| mtmd model sources | compiled in-tree | copied to `src/clip-models/` + GLOB |

**No structural changes** — only the per-encoder copy block was extended for the 5 new files.

---

## Action Items

1. **REQUIRED**: Rebuild the xcframework — `cd thirdparty/llama.cpp && ./build-xcframework-ios.sh`.
2. **Not required**: No session cache invalidation (host-memory state format unchanged).
3. **Recommended**: Smoke-test one multimodal model (e.g. a Gemma 4 / Granite 4 Vision GGUF) after rebuild to confirm the new encoders link and run.
