# llama.cpp Upgrade: b7402 -> b7549

**Upgrade Date:** 2025-12-26
**Total Commits:** 147

## Summary

This upgrade brings significant performance improvements, new model support, critical bug fixes, and enhanced multimodal capabilities. The changes span across Vulkan, CUDA, Metal backends, and core inference engine improvements.

---

## New Features

### Model Support (iOS-Suitable, <4B Parameters)

| Model | Size | Description |
|-------|------|-------------|
| **LFM2-Audio-1.5B (Conformer)** | 1.5B | ASR audio encoder with conformer architecture (#18106) |
| **GLM-ASR-Nano-2512** | ~500M | Nano-sized ASR model, fixed load error (#18130) |
| **Granite Embedding** | <1B | Text embedding model support (#15641) |

### New Multimodal Encoders

| File | Purpose | Use Case |
|------|---------|----------|
| **conformer.cpp** | Conformer audio encoder | Speech-to-text (ASR) for LFM2-Audio models |
| **glm4v.cpp** | GLM-4V vision encoder | Vision-language for GLM4V multimodal models |

### Architecture Improvements (All Model Sizes)

| Feature | Description |
|---------|-------------|
| **LlamaBidirectionalModel** | Bidirectional attention support (#18220) |
| **ARM64 q8_0 repack** | Optimized quantization for Apple Silicon (#18096) |

### Server Enhancements

- **Auto-sleep after idle** - Server can now auto-sleep after N seconds of idle (#18228)
- **Stop-timeout option for router** - Better control of shutdown (#18350)
- **Preset-only options** - Load model on startup with preset support (#18206)
- **Webui configuration** - New `--webui-config` option (#18028)
- **Router child process SSL disable** - Better security control (#18141)
- **Return progress at 0%** - Report processing state more accurately (#18305)
- **Editing attachments in user messages** - Improved webui functionality (#18147)

### New API Functions

```c
LLAMA_API bool llama_params_fit(...);          // New: Parameter fitting
LLAMA_API size_t llama_max_tensor_buft_overrides(void);  // New: Tensor buffer overrides
LLAMA_API void llama_log_get(...);             // New: Get current log callback
```

---

## Performance Improvements

### Vulkan Backend

| Improvement | Description |
|-------------|-------------|
| **mul_mat_id optimization** | Preprocess experts and discard workgroups more quickly (#18352) |
| **decodeFuncB optimization** | Coopmat2 mul_mat_id shader optimization (#18349) |
| **BK=32 for coopmat2** | Better block size for mul_mat_id (#18332) |
| **Small dequantization improvements** | General dequant performance (#18380) |
| **Fewer FA rows for small cache** | Flash attention optimization (#18280) |
| **ADD operations grouping** | Graph optimization improvement (#18060) |
| **Perf logger with concurrency** | New performance monitoring mode (#17944) |

### CUDA Backend

| Improvement | Description |
|-------------|-------------|
| **cumsum CUB path optimization** | Better cumulative sum performance (#18362) |
| **cumsum fallback kernel optimization** | Improved fallback path (#18343) |
| **Native MXFP4 support (Blackwell)** | Experimental 4-bit support for latest GPUs (#17906) |

### CPU Backend

| Improvement | Description |
|-------------|-------------|
| **RVV floating-point kernels** | RISC-V vector extension support (#17318) |
| **RVV sgemm kernels** | Llamafile RISC-V support (#18199) |
| **ARM64 q8_0 repack** | dotprod and i8mm optimizations (#18096) |

---

## Bug Fixes

### Critical Fixes

| Fix | Impact |
|-----|--------|
| **Server crash on seq_rm failure** | Hybrid/recurrent models crash fix (#18391) |
| **Server crash without BOS/EOS** | Model loading crash fix (#18321) |
| **Data race in HTTP threads** | Thread safety fix (#18263) |
| **Data race in to_json_anthropic** | Thread safety fix (#18283) |
| **RPC -fit compatibility** | Remote procedure call fix (#18233) |
| **Vulkan event_wait corruption** | Command buffer corruption fix (#18302) |

### Backend Fixes

| Fix | Backend |
|-----|---------|
| **Regex for arch list** | CUDA (#18371) |
| **Blackwell native builds** | CUDA (#18361) |
| **im2col overflow** | Vulkan (#18180) |
| **topk_moe with exp_probs_b** | Vulkan/CUDA (#18071) |
| **Rope with large number of rows** | Vulkan (#18306) |

---

## iOS/Metal Specific Changes

### Relevant for iOS Builds

1. **No breaking changes to Metal backend** - The Metal backend remains stable
2. **New vision model files** need to be included in custom builds:
   - `conformer.cpp` (new - for ASR support)
   - `glm4v.cpp` (new - for GLM model support)
3. **XCFramework workflow update** - Release workflow now stores XCFramework as Zip file (#18284)

### Multimodal (mtmd) Changes

- Updated mtmd context handling for server (#18106)
- Vision capability detection improvements in webui
- No API changes to clip.h or mtmd.h

---

## Build Script Changes Required

### New Vision Model Files to Include

The following files are **new** since b7402 and need to be added to `build-xcframework-ios.sh`:

```bash
# In copy_mtmd_files() function, add:
cp -fp "tools/mtmd/models/conformer.cpp" src/clip-models/
cp -fp "tools/mtmd/models/glm4v.cpp" src/clip-models/

# In sed patch for CMakeLists.txt, add to the list:
clip-models/conformer.cpp
clip-models/glm4v.cpp
```

### Header Updates

No new required headers for the framework. The current headers remain:
- `ggml-opt.h` (already included in both scripts)

---

## Risk Assessment

### High Risk

| Risk | Description | Mitigation |
|------|-------------|------------|
| **Missing vision models** | New `conformer.cpp` and `glm4v.cpp` not in build script | Update `copy_mtmd_files()` function |
| **API signature changes** | `llama_log_set` API changed, new `llama_log_get` added | Verify app code doesn't use old patterns |

### Medium Risk

| Risk | Description | Mitigation |
|------|-------------|------------|
| **Server behavior changes** | Auto-sleep, preset handling changed | Test server functionality if used |
| **Performance regression** | New optimizations may affect some models | Benchmark key models before release |

### Low Risk

| Risk | Description | Mitigation |
|------|-------------|------------|
| **Vulkan backend changes** | Many Vulkan optimizations (not used on iOS) | N/A for iOS builds |
| **CUDA backend changes** | CUDA improvements (not used on iOS) | N/A for iOS builds |
| **New model architectures** | New models may not work with older builds | Test specific models if needed |

---

## Recommended Actions Before Release

1. **Update build script** - Add new vision model files (conformer.cpp, glm4v.cpp)
2. **Rebuild XCFramework** - Run `./build-xcframework-ios.sh`
3. **Test key models** - Verify inference works correctly:
   - Text-only models (Llama, Qwen)
   - Vision models (LLaVA, Qwen2VL)
4. **Check for API usage** - Verify app doesn't call `llama_log_set` with old signature
5. **Benchmark performance** - Compare inference speed with previous build

---

## Changelog Highlights (Condensed)

```
vulkan: preprocess mul_mat_id experts optimization (#18352)
vulkan: optimize decodeFuncB in coopmat2 (#18349)
server: fix crash when seq_rm fails (#18391)
cuda: optimize cumsum cub path (#18362)
model: support MiMo-V2-Flash (#18328)
model: support LlamaBidirectionalModel (#18220)
model: add ASR support for LFM2-Audio-1.5B (#18106)
server: add auto-sleep after N seconds (#18228)
llama: Async DirectIO model loading on Linux (#18012)
ggml-cpu: ARM64 q8_0 repack optimization (#18096)
webui: Add editing attachments (#18147)
```

---

## Script Comparison: build-xcframework.sh vs build-xcframework-ios.sh

### Key Differences

| Aspect | Official Script | Custom Script (iOS) |
|--------|-----------------|---------------------|
| **Optimization flags** | `-g` only | `-O3 -fno-finite-math-only -g` |
| **Platforms built** | All (iOS, macOS, visionOS, tvOS) | iOS + macOS only |
| **mtmd files** | Not included | Copied from tools/mtmd to src |
| **Vision model headers** | Not in framework | clip.h, mtmd.h, mtmd-helper.h included |
| **Release optimization** | Default | `-DCMAKE_C_FLAGS_RELEASE="-O3 ..."` |

### Changes Needed in Custom Script

1. Add new vision model files to `copy_mtmd_files()`:
   ```bash
   cp -fp "tools/mtmd/models/conformer.cpp" src/clip-models/
   cp -fp "tools/mtmd/models/glm4v.cpp" src/clip-models/
   ```

2. Update sed patch for CMakeLists.txt to include:
   ```bash
   clip-models/conformer.cpp\
   clip-models/glm4v.cpp\
   ```

No other structural changes required to the custom build script.
