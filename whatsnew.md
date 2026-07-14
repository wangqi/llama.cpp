# llama.cpp Upgrade: b9870 → b9993

**Date:** 2026-07-13
**Commits in range:** 124 upstream commits merged

---

## New Features

### New Vision Models
- No new vision encoder `.cpp` files were added this cycle. All 33 encoders under `tools/mtmd/models/` are already wired into `build-xcframework-ios.sh`.
- `mtmd: deepseek-ocr v1 multi-tile (#24717)` — multi-tile support extends the existing DeepSeek-OCR encoder (no new file).

### New Text Model Architectures
| Model | PR | Notes |
|-------|-----|-------|
| Hunyuan v3 (hy_v3 / Hy3) | #25395 | New architecture with MTP speculative decoding |
| Minimax2 (eagle3 spec) | — | Eagle3 speculative-decoding draft support |

### Quantization
- `Add Q2_0 quantization: type definition and CPU backend (#24448)` — new `GGML_TYPE_Q2_0` / `LLAMA_FTYPE_MOSTLY_Q2_0` 2-bit type (CPU backend only; no Metal kernel yet).

### Metal / ARM64
- `metal : add CONV_2D_DW (depthwise convolution) support (#21565)`
- `metal: add col2im_1d op (f32/f16/bf16) (#25176)`
- `metal : add set_rows with src0 f16 (#25434)`
- `ggml-cpu: use UE4M3 LUT in ARM NVFP4 dot product (#25331)` — faster NVFP4 dot product on Apple Silicon.

### DeepSeek V3.2 / V4
- `ggml : add GGML_OP_LIGHTNING_INDEXER that implements DeepSeek V3.2/V4 lightning indexer (#24231)` — new op + `ggml_lightning_indexer()` API.
- `llama : make all KQ masks f16 if FA is used ... in DeepSeek V4 (#25370)`
- `llama: fix quantized kv-cache for dsv4 (#25202)`

### Chat Template / Tokenizer Fixes
- `chat : fix reasoning leak with force-opened bare <think> templates (#24674)`
- `server : move chat-template thinking probe inside the init try/catch (#24093)`

### Security / Robustness
- `mtmd: fix silent prompt truncation on embedded NUL (#25548)` — adds `text_len` to `mtmd_input_text` (see API Changes).
- `gguf : reject empty metadata keys (#24917)`
- `fix: OOB reads in UGM tokenizer (precompiled_charsmap handling) (#18750)`
- `ggml : fix broken CPU concat implementation for quantized types (#25247)`

### Third-party Library Updates
- ggml core: `0.15.3` → `0.16.0`
- cpp-httplib: `0.49.0` → `0.50.1` (#25576)
- `ggml-et: Initial ET backend (#24179)` — not built for iOS/macOS.

---

## API Changes

### `include/llama.h`
- **Added**: `LLAMA_FTYPE_MOSTLY_Q2_0 = 41` — new file-type enum for the Q2_0 quant. Additive; no impact.

### `ggml/include/ggml.h`
- **Added**: `GGML_TYPE_Q2_0 = 42`; `GGML_TYPE_COUNT` bumped `42` → `43`.
- **Added**: `GGML_FTYPE_MOSTLY_Q2_0 = 28`.
- **Added**: `GGML_OP_LIGHTNING_INDEXER` op + `ggml_lightning_indexer(...)` API.
- All additive. No removals.

### `ggml/include/gguf.h`
- **Added**: `gguf_get_tensor_ne(ctx, tensor_id)` — returns the `GGML_MAX_DIMS`-element `ne` array for a tensor (#24405). Additive.

### `tools/mtmd/mtmd.h` — BREAKING (struct layout)
- **Added**: `size_t text_len;` field inserted into `struct mtmd_input_text` between `text` and `add_special`.
- **Impact**: The Swift auto-generated memberwise initializer signature changed to `mtmd_input_text(text:text_len:add_special:parse_special:)`. Our wrapper's construction in `thirdparty/llamacpp_swift/Sources/swift/LLaMa_MModal.swift` would fail to compile until updated.
- **Applied fix**: line 680 now passes `text_len: strlen(cPrompt)` (matches upstream `mtmd-cli.cpp` which sets `text.text_len = formatted_chat.size()`).

### State Save/Load Behavioral Changes
- None. `llama_state_save_file` / `llama_state_load_file` behavior unchanged. Existing session cache files remain valid (no KV-cache format change affecting our models).

---

## Risk Assessment

### HIGH: `mtmd_input_text` struct layout change
**Problem:** A new `text_len` field was inserted into `mtmd_input_text`, changing the Swift memberwise initializer. Our multimodal tokenize path (`LLaMa_MModal.swift:680`) constructs this struct directly and would fail to compile.
**Required fix:** DONE — pass `text_len: strlen(cPrompt)`. Also fixes a latent bug: prompts containing embedded NUL bytes are no longer silently truncated.

### LOW: Q2_0 quantization is CPU-only
No Metal kernel for `GGML_TYPE_Q2_0` yet; Q2_0-quantized GGUFs would run on CPU. We ship no Q2_0 models, so no action required.

### LOW: PrismML Q1_0 quantization patch
The 1-bit Q1_0 Metal quant patch (`GGML_TYPE_Q1_0 = 41`, `GGML_FTYPE_MOSTLY_Q1_0 = 27`) is preserved: upstream's new `GGML_TYPE_Q2_0 = 42` slots in after it without displacing our markers. Verify `// wangqi modified` markers in `ggml/src/ggml-metal/ggml-metal-ops.cpp` after merge. No re-application needed.

### LOW: New ops (LIGHTNING_INDEXER, CONV_2D_DW, col2im_1d)
Additive ggml ops used only by specific architectures (DeepSeek V3.2/V4, depthwise-conv vision encoders). No impact on our shipped model set.

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| mtmd/clip encoders | Bundled via CMake target | Explicit `cp -fp` + sed CMakeLists patch of all 33 `clip-models/*.cpp` |

**No structural changes** — no new vision encoder `.cpp` files this cycle, so the copy block and sed patch list are unchanged.

---

## Action Items

1. **REQUIRED (DONE)**: Update `LLaMa_MModal.swift:680` for the new `mtmd_input_text.text_len` field.
2. **REQUIRED**: Rebuild the xcframework — `thirdparty/llama.cpp/build-xcframework-ios.sh`.
3. **Recommended**: Smoke-test a multimodal (vision) model to confirm mtmd tokenize still works after the struct change.
4. No session cache invalidation needed.
