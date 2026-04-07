# llama.cpp Upgrade: b8642 → b8690

**Date:** 2026-04-07
**Commits in range:** ~59 upstream commits merged

---

## New Features

### New Vision Models
- **HunyuanOCR** (`tools/mtmd/models/hunyuanocr.cpp`) — Tencent Hunyuan OCR-specialized vision encoder, added via PR #21395

### New Text Model Architectures
| Model | PR | Notes |
|-------|-----|-------|
| HunyuanOCR | #21395 | Vision-language OCR model from Tencent |

### Quantization
- **Q1_0 official support** (`ggml: add Q1_0 1-bit quantization support (CPU) #21273`) — Q1_0 is now in upstream ggml. Our custom PrismML Bonsai fork patch is superseded; this build uses the official implementation.

### Gemma 4 Fixes
Multiple Gemma 4 improvements landed in this range:
- `vocab: add byte token handling to BPE detokenizer for Gemma4 (#21488)`
- `llama: add custom newline split for Gemma 4 (#21406)`
- `common: add gemma 4 specialized parser (#21418)`
- `convert: set "add bos" == True for Gemma 4 (#21500)`
- `llama-model: read final_logit_softcapping for Gemma 4 (#21390)`

### Stability Fixes
- **Qwen2 segfault on long inputs**: `unicode: add custom Qwen2 regex handler to fix segfault on long input (#21257)` — previously crashed for very long prompts
- **KV cache / iSWA**: `kv-cache: support attention rotation for heterogeneous iSWA (#21513)` — fixes attention for interleaved sliding window attention architectures

---

## API Changes

### `ggml/include/ggml.h`
- **Deprecated**: `ggml_add1` and `ggml_add1_inplace` — now wrapped in `GGML_DEPRECATED(...)`, directing callers to use `ggml_add` / `ggml_add_inplace` instead. Low impact; these were rarely used directly in the iOS bridge.

### `include/llama.h`
- No breaking changes observed in this range.

### `tools/mtmd/mtmd.h`
- No breaking changes observed in this range.

### State Save/Load Behavioral Changes
- `server: fix restore for checkpoints with pos_min == 0 (#21510)` — fixes a server-side checkpoint edge case; no impact on iOS local inference.

---

## Risk Assessment

### LOW: Q1_0 custom patch superseded
The Bonsai/PrismML Q1_0 patch is now replaced by the official upstream implementation. Existing Q1_0 GGUF files should remain compatible. No action required, but test with a Bonsai-8B-gguf model after building.

### LOW: Gemma 4 tokenizer changes
Several Gemma 4 tokenizer/formatting fixes were applied. Existing Gemma 4 conversations may have slightly different formatting behavior. No action required.

### LOW: ggml_add1 deprecation
`ggml_add1` is deprecated. Not used in the iOS bridge layer (`thirdparty/llamacpp_swift`). No action required unless a future upstream removal causes a build warning.

---

## Build Script Changes

| File | Change |
|------|--------|
| `build-xcframework-ios.sh` | Added `cp -fp tools/mtmd/models/hunyuanocr.cpp src/clip-models/` |
| `build-xcframework-ios.sh` | Updated CMakeLists.txt sed guard to check for `hunyuanocr.cpp`; added to sed patch list |

**Verify**: `grep "hunyuanocr" thirdparty/llama.cpp/build-xcframework-ios.sh` should show 3 lines (copy, grep guard, sed patch).

---

## Action Items

1. **REQUIRED**: Run `./build-xcframework-ios.sh` to rebuild the XCFramework with HunyuanOCR encoder included.
2. **Recommended**: Test a Bonsai-8B Q1_0 model load to confirm the official Q1_0 path works identically to the custom patch.
3. **Recommended**: Test Gemma 4 chat to verify tokenizer and formatting fixes are working correctly.
