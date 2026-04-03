# llama.cpp Upgrade: b8565 → b8642

**Date:** 2026-04-02
**Commits in range:** ~77 upstream commits merged

---

## New Features

### New Vision Models
- **Gemma 4V** (`gemma4v.cpp`) — Gemma 4 multimodal vision encoder; added to build script copy block and CMakeLists.txt sed patch

### New Text Model Architectures
| Model | PR | Notes |
|-------|-----|-------|
| Gemma 4 | #21326 | Chat template fix for correct multi-turn behavior |
| Granite 4.0 | #20804 | Chat template with correct tool_call role mapping |

### KV Cache Improvements
- SWA (sliding window attention) KV cache no longer quantized — fixes correctness for models using SWA (e.g. Gemma 3 variants)

### Quantization
- Activation rotation before quantization (#21038) — improves output quality for quantized models

### mtmd Multimodal
- Fix GGUF conversion for audio/vision mmproj files (#21309)

### Jinja / Chat Templates
- Gemma 4 template fix — relaxed prefill parser to allow leading space (#21240)
- Granite 4.0 chat template with correct `tool_call` role mapping (#20804)
- Fix tool call parsing for LFM2 and LFM2.5 models (#21242)

---

## API Changes

### `include/llama.h`
- **Added** `struct llama_model_tensor_override { const char* pattern; enum ggml_type type; }` — typed tensor type overrides replacing old `void* tensor_types`
- **Added** `struct llama_model_imatrix_data { const char* name; const float* data; size_t size; }` — typed imatrix struct replacing old `void* imatrix`
- **Changed** `llama_model_quantize_params.imatrix`: `void*` → `const struct llama_model_imatrix_data*`
- **Changed** `llama_model_quantize_params.kv_overrides`: `void*` → `const struct llama_model_kv_override*`
- **Changed** `llama_model_quantize_params.tensor_types` → renamed to `tt_overrides`: `void*` → `const struct llama_model_tensor_override*`
- **Changed** `llama_model_quantize_params.prune_layers`: `void*` → `const int32_t*`

**Impact:** Pure C interface — all opaque `void*` pointers replaced with typed structs. We do not call `llama_model_quantize` from the app; no action required.

---

## Risk Assessment

### LOW: API quantize_params refactor
Typed replacement of `void*` fields. We do not call quantize from the app; no action required.

### LOW: SWA KV cache no longer quantized
Correctness fix — may slightly increase memory for models with SWA layers, but improves output quality.

### LOW: Gemma 4V build script patch
`gemma4v.cpp` added to both the `cp` block and the CMakeLists.txt sed guard. Grep guard updated to check for `gemma4v.cpp`.

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| New file | `gemma4v.cpp` added upstream | Added to copy block and sed patch |

**No structural changes** to the build script beyond the new file addition.

---

## Action Items

1. **REQUIRED**: Rebuild XCFramework to include `gemma4v.cpp` — run `./build-xcframework-ios.sh`
2. **Recommended**: Test Gemma 4 text generation (template fix)
3. **Recommended**: Test any SWA-based model (Gemma 3 series) for quality improvement from KV cache fix
