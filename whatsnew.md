# llama.cpp Upgrade: b9754 → b9870

**Date:** 2026-07-03
**Commits in range:** 117 upstream commits merged

---

## New Features

### New Text Model Architectures

| Model | PR | Notes |
|-------|-----|-------|
| DeepSeek V4 | #24162 | Full new architecture: converter (dsv4), `llm_graph_input_dsv4`, save/load state, Sinkhorn eps routing, RoPE fix, chat template, and Pro-model variant support |
| MiniCPM 5 | #24889 | New chat/reasoning parser for the MiniCPM 5 family |
| LFM2.5-230M | #25008 | Architecture label registered for the 230M dense variant |
| LFM2.5-ColBERT-350M / LFM2.5-Embedding-350M | #24913 | New Liquid AI retrieval/embedding models |
| Qwen3-Next | #25141 | `t_layer_inp` tensor registered — fixes graph wiring for Qwen3-Next |

### New Audio / Vision Models

- **Granite Speech Plus** (#24818) — extended Granite speech encoder path.
- **Unlimited-OCR** (#24969) — converter plus parity test for the unlimited-context OCR multimodal path.

### Speculative Decoding

- **DFlash draft support** (#22105) with `--spec-draft-p-min` acceptance control (#25246) and a refactored draft-model conversion path (#25110).
- **Eagle3 Qwen3 draft models** documented and supported (#24977).

### Multimodal (mtmd)

- Additional input validations in mtmd to reject malformed clip inputs (#25013).
- `libmtmd` is now bundled into the Apple XCFramework (#21935).
- mtmd video is disabled on iOS / tvOS / visionOS in the official xcframework build (#25018) — matches our iOS build, which does not ship video.

### Engine / Third-party

- ggml core bumped **0.15.2 → 0.15.3** (ggml/1550).
- cpp-httplib updated to **0.49.0** (#25218).
- Quantization fix for MoE models with MTP tensors (#24986).

---

## API Changes

### `include/llama.h`

- **Added**: `LLAMA_API const char * llama_ftype_name(enum llama_ftype ftype);` — returns a human-readable name for a file type.
- **Added**: `LLAMA_API enum llama_ftype llama_model_ftype(const struct llama_model * model);` — queries a loaded model's file type.

Both are purely additive (non-breaking). No fields removed or signatures changed. `ggml.h`, `gguf.h`, `mtmd.h`, and `clip.h` are unchanged in this range.

### State Save/Load Behavioral Changes

- None. DeepSeek V4 adds its own save/load-state handling internal to that architecture; existing session cache files for previously supported models remain valid.

---

## Risk Assessment

### LOW: DeepSeek V4 new architecture
Additive new model path; does not affect existing models. No action required unless shipping a DSV4 GGUF.

### LOW: New `llama_ftype_name` / `llama_model_ftype` APIs
Additive C API. Our Swift bridge does not need to adopt them. No action required.

### LOW: mtmd validations tightened
Stricter clip-input validation could reject previously-tolerated malformed inputs, but our bundled multimodal models produce well-formed inputs. No action required.

### LOW: PrismML Q1_0 quantization patch
Verified intact after merge: `GGML_TYPE_Q1_0 = 41` and `GGML_FTYPE_MOSTLY_Q1_0 = 27` remain in `ggml/include/ggml.h`; `// wangqi modified` markers remain in `ggml/src/ggml-metal/ggml-metal-ops.cpp`. No re-application needed.

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| mtmd video | disabled on i/tv/visionos (#25018) | not shipped (already excluded) |
| clip-models | CMake-driven | manual copy + sed patch of `src/CMakeLists.txt` |

**No structural changes needed.** No new `.cpp` files appeared in `tools/mtmd/models/` in this range — every encoder is already in the copy block and sed patch list. The build script is unchanged.

---

## Action Items

1. **None required before building.** No new vision encoders, no build-script edits, PrismML patch intact.
2. **Recommended**: smoke-test one existing GGUF (text) and one multimodal GGUF after the xcframework rebuild to confirm the ggml 0.15.3 bump is clean on Metal.
