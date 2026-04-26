# llama.cpp Upgrade: b8843 → b8933

**Date:** 2026-04-25
**Commits in range:** 86 upstream commits merged

---

## New Features

### New Vision Models
- **Reka Edge 2603** (`yasa2.cpp`) — new vision encoder added via `mtmd: Add support for Reka Edge 2603 (#21616)`

### Metal / GPU Improvements
- **Metal Tensor API optimized** for `GGML_OP_MUL_MAT` — reduced kernel dispatch overhead on Apple Silicon (#20962)
- **Metal event synchronization fix** — corrects synchronization between GPU passes (#22260)
- **macOS GPU interactivity watchdog workaround** — prevents macOS from throttling GPU during long inference runs (#22216)
- **Metal: GPU description logging** — prints device name on load (#22318)

### Vision / Multimodal
- **HunyuanVL support updated** (#22037) — improved vision-language model pipeline
- **`LLAMA_ROPE_TYPE_NONE` support in mtmd** (#22242) — broadens compatibility for models without RoPE
- **M-RoPE position decoding fixes** — `get_n_pos` / `get_decoder_pos` corrected (#22175), `mtmd_decode_use_mrope()` corrected (#22188)

### Server / API
- **CVE-2026-21869 security fix** — heap-buffer-overflow from negative `n_discard` (#22267)
- **LFM2-Audio transcriptions API support** (#22000)
- **Allow cancel loading model** (#21814)
- **Anthropic API prefix caching fix** (#21793)
- **`chat: fix parallel_tool_calls` default** (#22217)

---

## API Changes

### `include/llama.h`
- **Removed**: `llama_params_fit()` and `llama_params_fit_status` enum — no longer in the public API
- **Removed**: `llama_memory_breakdown_print()` — removed from public header

### `tools/mtmd/mtmd.h`
- **Changed**: The following functions now take `const mtmd_context *` instead of `mtmd_context *`:
  - `mtmd_decode_use_non_causal()`
  - `mtmd_decode_use_mrope()`
  - `mtmd_support_vision()`
  - `mtmd_support_audio()`
  - `mtmd_get_audio_sample_rate()`
  - Impact: Swift bridge passes context opaquely — no source changes needed in `LLaMa_MModal.swift`.

---

## Risk Assessment

### MEDIUM: `llama_params_fit` removed
**Problem:** Any code calling `llama_params_fit()` will fail to compile.
**Mitigation:** This function was never called in our Swift bridge (only used in llama.cpp server tools).

### LOW: mtmd const-correctness changes
Functions querying mtmd_context now require `const` — no callers affected in our bridge.

### LOW: Metal event sync fix
Correctness fix only; no behavior change visible to Swift callers.

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| Model sources | Manual list (may differ) | `file(GLOB)` via `clip-models/*.cpp` (auto-inclusive since b8843) |
| New model added | `yasa2.cpp` | Added to copy block (2026-04-25) |

**No structural changes needed** — the `file(GLOB)` pattern in `src/CMakeLists.txt` auto-picks up `yasa2.cpp` once copied.

---

## Action Items

1. **REQUIRED**: Rebuild xcframework: `cd thirdparty/llama.cpp && ./build-xcframework-ios.sh`
2. **Recommended**: Smoke-test a vision model (Qwen3-VL or LLaVA) and a text model after the rebuild
3. **Optional**: Verify Reka Edge 2603 GGUF works if a quantized file is available
