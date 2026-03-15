# llama.cpp Upgrade: b8240 → b8355

**Date:** 2026-03-15
**Commits in range:** ~60 upstream commits merged

---

## New Features

### New Vision Models
No new vision encoder `.cpp` files were added in this range. All 18 existing encoders (cogvlm, conformer, glm4v, internvl, kimik25, kimivl, llama4, llava, minicpmv, mobilenetv5, nemotron-v2-vl, paddleocr, pixtral, qwen2vl, qwen3vl, siglip, whisper-enc, youtuvl) are already included in `build-xcframework-ios.sh`.

### New Text Model Architectures
No entirely new model architectures were introduced in this range.

---

## Key Changes

### NVFP4 Quantization: Qwen3.5 and Qwen3.5 MoE (#20506)
- `ggml.h` adds `GGML_TYPE_NVFP4 = 40` and `GGML_FTYPE_MOSTLY_NVFP4 = 26`
- `llama.h` adds `LLAMA_FTYPE_MOSTLY_NVFP4 = 39`
- Qwen3.5 and Qwen3.5 MoE tensor wiring added for NVFP4 loading
- Impact: NVFP4-quantized Qwen3.5 GGUF files can now be loaded. No impact on existing models.

### Metal: Flash Attention Specialization for HSK=320, HSV=256 (#20549)
- New Metal kernel variant for Flash Attention with head-state-key=320, head-state-value=256
- Covers models such as Llama 4 Scout/Maverick which use those non-standard head dimensions
- Impact: Faster inference for affected models on Apple Silicon; other models unaffected.

### Metal: Correctness Fixes (#20493, #20426)
- **l2 norm scale fix** (#20493): Fixed incorrect scale factor in Metal l2_norm kernel — model quality improvement for models using l2 normalization layers
- **bin kernel optimization** (#20426): Removed divisions from the Metal bin kernel; replaced with multiply-by-reciprocal for correct rounding and throughput improvement

### KV Cache: Fix State Read Regression (#20273)
- Fixed incorrect reading of `llama_kv_cell_ext` fields during KV cache state restore
- Affects use of `llama_state_load_file()` / session cache restore — corrupted ext fields no longer silently ignored
- Impact: Session cache reliability improvement; relevant when restoring multi-turn context

### Tool Calling: Graceful Undetected Parser Handling (#20286)
- `common/parser` now prints a clear error and recovers when no tool-call parser is detected, rather than silently failing
- Impact: Better error reporting for tool-call misconfiguration; no behavioral change when parser is correctly set

### GDN: Crash Fix for Chunked Pooling (#20468)
- `llama : fix pooling assertion crash in chunked GDN detection path`
- Prevents assertion failure when GDN (Gated Delta Net) models are run with chunked batch processing
- Impact: Stability fix for hybrid SSM+attention models (Falcon H1-style) under batch inference

### mtmd API Rename: Audio Sample Rate (#20105)
- `mtmd_get_audio_bitrate()` renamed to `mtmd_get_audio_sample_rate()` — name was misleading; the value is in Hz (sample rate), not bits/sec (bitrate)
- Internal references in `mtmd-helper.cpp` updated accordingly
- Impact on iOS app: The framework headers will update on next XCFramework rebuild. Swift call sites in `llamacpp_swift` that used the old name need updating.

### GATED_DELTA_NET Op: Metal Not Yet Supported (#20455, #20334)
- `GGML_OP_GATED_DELTA_NET` introduced in previous range; Vulkan backend gained support this range
- Metal backend support not yet added — hybrid SSM models on Apple Silicon fall back to CPU for this op
- No crash; performance may be reduced on affected models

---

## API Changes

### `ggml/include/ggml.h`
- **Added**: `GGML_TYPE_NVFP4 = 40` (NVFP4 quantization; 4 blocks, E4M3 scale)
- **Changed**: `GGML_TYPE_COUNT = 41` (was 40)
- **Added**: `GGML_FTYPE_MOSTLY_NVFP4 = 26`

### `include/llama.h`
- **Added**: `LLAMA_FTYPE_MOSTLY_NVFP4 = 39`

### `tools/mtmd/mtmd.h`
- **Renamed**: `mtmd_get_audio_bitrate()` → `mtmd_get_audio_sample_rate()`
- **Comment updated**: doc clarifies return value is sample rate in Hz, not bitrate

---

## Build Script

No changes required to `build-xcframework-ios.sh` for this upgrade. All 18 vision encoder files are already listed in the `copy_mtmd_files()` section and the CMakeLists.txt sed patch.

| Aspect | Status |
|--------|--------|
| New vision `.cpp` files | None |
| Build script patch needed | Yes — `src/debug/mtmd-debug.h` must be copied (mtmd.cpp now includes it) |
| New cmake flags needed | No |

---

## Risk Assessment

| Risk | Level | Description |
|------|-------|-------------|
| NVFP4 new quant type | LOW | Additive only; `GGML_TYPE_COUNT` bumped to 41. Existing models unaffected. |
| Metal l2 norm scale fix | LOW | Correctness fix; model output may change slightly for affected models (improvement). |
| KV cache state read fix | LOW | Correctness fix; session cache restores more reliable. No format change. |
| `mtmd_get_audio_sample_rate` rename | LOW | Source-level rename. XCFramework rebuild picks up new name automatically; any direct Swift callers of old name must be updated. |
| GDN pooling crash fix | LOW | Stability improvement only. |
| Flash Attention HSK=320 specialization | LOW | Additive Metal kernel. No regression risk for existing models. |
