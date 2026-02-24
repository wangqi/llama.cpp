# llama.cpp Upgrade: b8089 → b8145

**Date:** 2026-02-24
**Commits in range:** 56 upstream commits merged

---

## New Features

### New Vision Model: PaddleOCR-VL
- Added `tools/mtmd/models/paddleocr.cpp` — vision encoder for PaddleOCR-VL
- Uses M-RoPE (multi-dimensional rotary position embedding) for 4D position IDs
- ⚠️ **Build script action required** — see Risk section below

### GLM-OCR Support via glm4v
- Extended `tools/mtmd/models/glm4v.cpp` to support GLM-OCR models
- GLM-OCR does not have learned position embeddings; the build now handles the `nullptr` case gracefully
- No new file added; existing `glm4v.cpp` already included in our build script

### Flash Attention Toggle in mtmd (via ctx_params)
- `mtmd: build_attn modified, flash_attn on/off via ctx_params (#19729)`
- Flash attention for multimodal vision encoders can now be controlled via the llama context parameters
- Behavioral change: flash attention routing is now delegated to the context params rather than hardcoded

### New Text Model Architectures
| Model | PR | Notes |
|-------|-----|-------|
| Kanana-2 | #19803 | New Korean LLM architecture |
| JAIS-2 | #19488 | Arabic-English bilingual LLM |
| Full Modern BERT | #18330 | Complete BERT encoder support |
| LFM2.5-Audio-1.5B | #19687 | Tokenizer added for audio model |
| LFM2-24B-A2B label | #19848 | Label update only |

### Quantization: `--dry-run` Option
- Added `dry_run` field to `llama_model_quantize_params` in `include/llama.h`
- Allows calculating the final quantization size without performing the actual quantization
- Low risk: additive API change, default value is `false`

### ARM64 CPU Performance Improvement
- `ggml-cpu: arm64: q5_K repack GEMM and GEMV (dotprod) (#19356)`
- Significant performance improvement for Q5_K quantized models on Apple Silicon
- No API change; pure performance optimization

### Jinja Template Improvements
- Fixed stats for `tojson` and `string` filters (#19785)
- Added `"indent"` string filter support (#19529)
- Fixed Step-3.5-Flash format detection and thinking support (#19635)
- Fixed gpt-oss Jinja error when assistant message has both content and thinking with tool calls (#19704)
- Improved Qwen3-Coder and Nemotron Nano 3 chat template parsers (#19765)

### Server Enhancements
- `max_completion_tokens` request property now supported (#19831)
- Contiguous Responses API input items merged into single assistant message (#19773)
- Slots debug endpoint saves generated text when `LLAMA_SERVER_SLOTS_DEBUG=1` (#19622)

### Third-Party Updates
- `cpp-httplib` updated from 0.33.1 → 0.34.0 (#19830)

---

## API Changes

### `include/llama.h`
- **Added**: `bool dry_run` field in `llama_model_quantize_params` struct
  - Low risk: new field with `false` default; no Swift bridge changes needed

### `ggml/include/ggml.h`
- **Removed**: `ggml_type_sizef()` deprecated function (previously marked `GGML_DEPRECATED`)
  - This function was deprecated for some time; callers should use `ggml_row_size()` instead
  - The `llamacpp_swift` bridge does **not** use this function — no impact on our build

### State Save/Load Behavior Change (`src/llama.cpp`)
- **PR #18862**: Removed write/read of output ids, logits, and embeddings from `llama_state_save_file` / `llama_state_load_file`
  - Logits and embeddings are **no longer persisted** in saved sessions
  - Saved state files from b8145 are **not backward compatible** with files saved by earlier builds
  - Apps using session caching/resumption will need to re-run the final token after loading state to regenerate valid logits
  - Our `llamacpp_swift` bridge calls both `llama_state_save_file` and `llama_state_load_file` — **session files created before this upgrade must be discarded**

---

## Risk Assessment

### HIGH: `paddleocr.cpp` Missing from Build Script

**Problem:** `tools/mtmd/models/paddleocr.cpp` is a new file added in this upgrade, but it is **not** listed in `build-xcframework-ios.sh`'s `copy_mtmd_files()` function or the CMakeLists.txt sed-patch. The linker will report `Undefined symbols: vtable for clip_graph_paddleocr::build()` at link time.

**Required fix:** Add to `build-xcframework-ios.sh`:
```bash
# In copy_mtmd_files(), add:
cp -fp "tools/mtmd/models/paddleocr.cpp" src/clip-models/

# In the sed patch list, add after nemotron-v2-vl.cpp:
clip-models/paddleocr.cpp\
```

### MEDIUM: State File Incompatibility

**Problem:** Session state files saved with llama.cpp before b8145 no longer contain logits/embeddings. Loading such old files and continuing inference without re-evaluating the last token will produce garbage next-token predictions.

**Impact:** Any user who has a cached session file from a previous version will get incorrect inference results on session resume until the state is regenerated.

**Mitigation:** Invalidate all cached session files after upgrade. The `llamacpp_swift` bridge already re-evaluates the last prompt token after loading state (`GPT_SPM.swift:561`), so behavior may be acceptable — verify with testing.

### MEDIUM: mtmd Flash Attention Routing Change

**Problem:** Flash attention for multimodal vision is now controlled by `ctx_params` rather than hardcoded. The behavior depends on how the llama context is created. If `llamacpp_swift` creates contexts with flash attention disabled, multimodal encoding performance may regress on Apple Silicon.

**Mitigation:** Verify that multimodal (CLIP/mtmd) inference still works correctly after rebuild. Benchmark latency on image encoding.

### LOW: `ggml_type_sizef` Removal

The deprecated `ggml_type_sizef()` function has been removed. Our `llamacpp_swift` bridge does not use it. No action required.

### LOW: New Model Architectures

New model families (Kanana-2, JAIS-2, BERT, PaddleOCR-VL) were added. These only activate when the corresponding GGUF model files are loaded. No regression risk for existing GGUF models.

---

## Build Script Comparison: `build-xcframework.sh` vs `build-xcframework-ios.sh`

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| mtmd/clip files | Not copied (not needed) | Copied via `copy_mtmd_files()` |
| Mac Catalyst | Not supported | Added (arm64 + x86_64 via lipo) |
| Optimization flags | No explicit `-O3` | `-O3 -fno-finite-math-only` |
| OpenSSL | Not in COMMON_CMAKE_ARGS | `-DLLAMA_OPENSSL=OFF` pre-set |
| New headers exported | Includes `ggml-opt.h` | Same (already present) |
| Module map | Does NOT include clip/mtmd | Includes `clip.h`, `mtmd.h`, `mtmd-helper.h` |

**No structural changes** to the official build script that require migration to our custom script. The official script is functionally identical for iOS/macOS build logic.

---

## Action Items

1. **REQUIRED before building**: Add `paddleocr.cpp` to `build-xcframework-ios.sh` (`copy_mtmd_files()` and CMakeLists.txt sed patch)
2. **REQUIRED after building**: Invalidate any cached session state files to avoid stale logits from old saves
3. **Recommended**: Test multimodal inference after rebuild to verify flash attention routing works correctly
