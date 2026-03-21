# llama.cpp Upgrade: b8355 → b8461

**Date:** 2026-03-21
**Commits in range:** 99 upstream commits merged

---

## New Features

### New Vision Models
- No new vision encoder `.cpp` files added in this range. All encoders (`cogvlm`, `conformer`, `glm4v`, `internvl`, `kimivl`, `kimik25`, `llama4`, `llava`, `minicpmv`, `mobilenetv5`, `nemotron-v2-vl`, `paddleocr`, `pixtral`, `qwen2vl`, `qwen3vl`, `siglip`, `whisper-enc`, `youtuvl`) were already present.

### New Text Model Architectures
| Model | PR | Notes |
|-------|-----|-------|
| Mistral Small 4 | #20649 | New model type detection for Mistral Small 4 |

### mtmd Improvements
- `mtmd: add clip_graph::build_mm()` (#20751) — new multimodal graph builder for improved vision encoding pipelines
- `server: improve mtmd ctx checkpoints` (#20726) — better checkpoint management for multimodal context across server calls

### LoRA Adapter Management
- `llama: re-enable manual LoRA adapter free` (#19983) — `llama_adapter_lora_free()` is no longer deprecated; adapters not manually freed will be freed with their model

### Control Vectors
- `model: add control vector support where missing` (#20653) — control vectors now supported across more model architectures

### Server Enhancements
- `server: Add cached_tokens info to oaicompat responses` (#19361) — cached token count now visible in OpenAI-compatible responses
- `tools/server: support refusal content for Responses API` (#20285) — refusal messages handled in Responses API

---

## API Changes

### `include/llama.h`
- **Restored**: `llama_adapter_lora_free()` — previously deprecated, now reinstated as a valid manual free function. Adapters not freed manually will still be freed when the model is destroyed.

### `ggml/include/ggml.h`
- **Deprecated**: `ggml_type_sizef(enum ggml_type type)` — use `ggml_row_size()` instead. A `GGML_DEPRECATED` macro now wraps it.
- **Version bump**: ggml bumped to 0.9.8

### State Save/Load Behavioral Changes
- `context: zero output buffer on allocation` (#20781) — output buffers are now zero-initialized; reduces risk of stale data in partially-used context slots. No session cache invalidation required.

---

## Risk Assessment

### MEDIUM: ggml_type_sizef Deprecation
**Problem:** `ggml_type_sizef()` is now wrapped in `GGML_DEPRECATED` — compilers may emit warnings if used in our code.
**Mitigation:** Search codebase for any direct calls; replace with `ggml_row_size()`. Our Swift wrapper (`llamacpp_swift`) is unlikely to call this directly, but verify after building.

### LOW: LoRA Free Reinstatement
`llama_adapter_lora_free()` is no longer deprecated. No behavior change for our app — we do not call it directly, and adapters are freed with the model.

### LOW: Jinja Parser Fixes
`jinja: fix heap OOB read in value equality comparison` (#20782) and `common/parser: fix out_of_range crash in throw path` (#20777) are stability-only fixes with no API changes.

### LOW: Common Parser Prompt Corruption Fix
`common/parser: fix nasty bug causing subtle corruption of generation prompt` (#20825) — only affects models using GPT-OSS or similar parsers; no action needed.

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| Model files | All 18 encoders present | All 18 encoders present — in sync |

**No structural changes** needed to `build-xcframework-ios.sh` — all vision encoder `.cpp` files were already added in previous upgrades.

---

## Action Items

1. **Recommended**: Build xcframework and run a local vision model test to verify `clip_graph::build_mm()` integration is stable.
2. **Recommended**: Check `llamacpp_swift` and any bridging code for calls to `ggml_type_sizef` and replace with `ggml_row_size()` if found.
3. **No session cache invalidation required** — `context: zero output buffer` does not change the serialized format.
