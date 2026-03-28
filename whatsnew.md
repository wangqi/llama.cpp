# llama.cpp Upgrade: b8461 → b8565

**Date:** 2026-03-28
**Commits in range:** 110 upstream commits merged

---

## New Features

### New Vision Models
- **DeepSeekOCR** (`deepseekocr.cpp`) — M-RoPE-based OCR vision encoder for DeepSeek-OCR models; supports quantized `v.patch_embd` and Metal-compatible im2col ops

### New Text Model Architectures
| Model | Commit | Notes |
|-------|--------|-------|
| F2LLM-v2 (codefuse-ai) | `80322ebda` | CodFuse AI multilingual code model |

### Reasoning Improvements
- `reasoning_content` field now sent back to model across turns via API (`d0fa2c9fb`)
- `reasoning_format = none` support added for gpt-oss (`e6f2ec01f`)
- Lazy grammar sampler inhibited while reasoning is active (`59d840209`)

### Server Enhancements
- Built-in tools backend support added (`20197b6fe`)
- Custom socket options to disable `SO_REUSEPORT` (`5c1a7b835`)

### API / Library Updates
- `cpp-httplib` updated to 0.40.0 (`b0f0dd3e5`)

---

## API Changes

### `ggml/include/gguf.h`
- **Added**: `gguf_init_from_file_ptr(FILE *, gguf_init_params)` — load GGUF from an open FILE pointer
- **Added**: `gguf_write_to_file_ptr(const gguf_context *, FILE *, bool)` — write GGUF to an open FILE pointer

### `include/llama.h`
- **Added**: `llama_model_load_from_file_ptr(FILE *, llama_model_params)` — load model from an open FILE pointer

### mtmd / clip
- `mtmd: refactor image preprocessing` (`a73bbd5d9`) — internal restructuring, no API surface changes expected
- `mtmd: add more sanity checks` (`871f1a2d2`) — added bounds/type validation in clip pipeline

---

## Risk Assessment

### LOW: DeepSeekOCR vision encoder + new mtmd-image split
New encoder `deepseekocr.cpp` added to `tools/mtmd/models/`. Additionally, `mtmd.cpp` was refactored to split image preprocessing into `mtmd-image.h` / `mtmd-image.cpp`. Build script patched: both files are now copied to `src/`, and a new sed guard inserts `mtmd-image.cpp` into the `src/CMakeLists.txt` source list (it is absent from the checked-in file). Without this patch the build fails with `fatal error: 'mtmd-image.h' file not found`.

### LOW: Reasoning API field `reasoning_content`
New field sent back to model. If the app parses assistant messages for tool calls, verify that `reasoning_content` in the response doesn't interfere with tool-call extraction logic.

### LOW: FILE pointer APIs
Three new FILE-pointer-based load/write APIs added. No impact on existing code paths that use path-based loading.

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| New model files | N/A — cmake-native | Added `deepseekocr.cpp` to `copy_mtmd_files()` and sed CMakeLists.txt patch |

**No structural changes** to the build script layout required for this upgrade.

---

## Action Items

1. **REQUIRED**: Rebuild xcframework — `deepseekocr.cpp` must be compiled in for DeepSeek-OCR multimodal support
2. **Recommended**: Test an existing vision model (e.g. Qwen2-VL) to verify the mtmd image preprocessing refactor (`a73bbd5d9`) doesn't affect inference quality
3. **Recommended**: Verify reasoning-capable models (DeepSeek-R1, QwQ) still produce correct multi-turn output with `reasoning_content` round-tripped
