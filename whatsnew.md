# llama.cpp Upgrade: b8763 → b8843

**Date:** 2026-04-19
**Commits in range:** ~83 upstream commits merged

---

## New Features

### New Audio/Vision Encoders
- **qwen3a.cpp** — Qwen3 audio encoder supporting qwen3-omni and qwen3-asr models (PR #19441)

### New Text Model Architectures
No new text model architectures in this range.

### Metal / ARM64
- Metal: Fixed Flash Attention support logic (`metal: fix FA support logic`)
- Metal: Added XIELU unary op (`metal: add XIELU unary op`)
- Metal: Implemented ROLL op (`metal: Implement ROLL op`)

### mtmd / Vision
- `mtmd_image_tokens_get_decoder_pos()` new API for M-RoPE decoder position (replaces `get_nx`/`get_ny`)
- `mtmd_decode_use_non_causal()` signature updated: now takes `const mtmd_input_chunk * chunk` param
- Fixed crash when sending image under 2x2 pixels (`mtmd: fix crash when sending image under 2x2 pixels`)
- Gemma 4 audio now uses causal attention (`mtmd: use causal attn for gemma 4 audio`)
- Added `pos_0` parameter to `mtmd_image_tokens_get_decoder_pos` (breaking change in mtmd API)

### Server
- Speculative checkpointing added (`server: speculative checkpointing`)
- OAI `/v1/audio/transcriptions` API support (`server: support OAI /v1/audio/transcriptions API`)
- Random media marker support (`server: use random media marker`)

### Build / CMake
- Upstream now uses `file(GLOB LLAMA_MODELS_SOURCES "models/*.cpp")` — our merge conflict resolved to adopt this approach plus `file(GLOB LLAMA_CLIP_MODEL_SOURCES "clip-models/*.cpp")` for clip models. Build script `sed` patch for CMakeLists.txt removed (glob handles it).

### Other
- Download cancellation and temp file cleanup (`common: add download cancellation and temp file cleanup`)
- Model: single `llm_build` per arch refactor (`model: using single llm_build per arch`)
- DeepSeek v3.2 dedicated chat parser + official template
- `ggml_rope` docs significantly expanded (RoPE documentation)

---

## API Changes

### `tools/mtmd/mtmd.h`
- **Changed**: `mtmd_decode_use_non_causal(ctx)` -> `mtmd_decode_use_non_causal(ctx, chunk)` — extra `chunk` param; pass `nullptr` for image (existing behavior). **Our Swift code does not call this directly — no change needed.**
- **Deprecated**: `mtmd_image_tokens_get_nx()` and `mtmd_image_tokens_get_ny()` — marked `DEPRECATED`, use `mtmd_image_tokens_get_decoder_pos()` instead. **Our Swift code does not call these — no action needed.**
- **Added**: `struct mtmd_decoder_pos { t, x, y, z }` and `mtmd_image_tokens_get_decoder_pos(tokens, pos_0, i)` for M-RoPE decoder position.
- **Removed**: `MTMD_DEFAULT_IMAGE_MARKER` macro (deprecated in prior release).

### `ggml/include/ggml.h`
- RoPE documentation block significantly expanded (NORMAL, NEOX, MROPE, IMROPE, VISION modes documented). No API signature changes.

---

## Risk Assessment

### MEDIUM: mtmd_decode_use_non_causal signature change
**Problem:** Function takes an extra `const mtmd_input_chunk * chunk` parameter. Any direct C++ caller that uses the old 1-arg form will fail to compile.
**Mitigation:** Our Swift wrapper (`LLaMa_MModal.swift`) does not call this function directly. No Swift-side changes required. Verify after xcframework rebuild that it compiles cleanly.

### LOW: mtmd_image_tokens_get_nx / get_ny deprecated
Old APIs still compile (only marked deprecated). No build errors expected. No action required.

### LOW: MTMD_DEFAULT_IMAGE_MARKER removed
Was deprecated since before b8763. `mtmd_default_marker()` is the replacement. Our code does not use the macro — confirmed by grep.

### LOW: cmake glob for models
`src/CMakeLists.txt` now globs `models/*.cpp` and `clip-models/*.cpp`. New upstream model files are auto-included on next build. No action required unless a file needs to be excluded.

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| clip-models copy | N/A (not in official script) | Copies all encoders to src/clip-models/ |
| CMakeLists patch | N/A | Removed in b8843 (glob now handles it) |

**Structural change:** Removed the `sed` patch block that explicitly listed clip-models in CMakeLists.txt. `file(GLOB)` now handles this automatically.

---

## Action Items

1. **REQUIRED**: Run `./build-xcframework-ios.sh` to rebuild the xcframework with qwen3a.cpp included.
2. **Recommended**: Smoke-test a vision model (e.g. Qwen3-VL or LLaVA) and an audio model (Whisper) after the rebuild.
3. **No session cache invalidation needed** — no state save/load format changes in this range.
