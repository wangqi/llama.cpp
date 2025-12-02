# llama.cpp Upgrade: b7140 -> b7222

**Upgrade Date:** 2025-12-01
**Total Commits:** 82
**Risk Level:** LOW to MEDIUM

---

## Summary

This upgrade includes significant ARM CPU performance improvements, Metal backend enhancements, new model architecture support, and API additions for model-embedded sampling parameters. The changes are primarily additive with no breaking API changes detected.

---

## iOS/Apple-Specific Changes

### Metal Backend (GPU)

| Commit | Description |
|--------|-------------|
| `649495c9d` | **Add Flash Attention head size 48** - Enables FA optimization for models with 48-dimension attention heads |

**Files Changed:** `ggml-metal-device.m`, `ggml-metal.metal`

### ARM CPU Performance

| Commit | Description |
|--------|-------------|
| `dbb852b54` | **q4_K repack GEMM/GEMV with i8mm** - Major performance boost using ARM i8mm instructions for Q4_K quantization |
| `cd8370b40` | **q4_K repack GEMM/GEMV with dotprod** - Performance improvement using ARM dotprod instructions |
| `e6923caae` | **Fix ARM feature verification** - Fixes CMake feature detection for dotprod, SVE, i8mm on ARM64 |

**Impact:** These changes provide substantial inference speed improvements on Apple Silicon (M1/M2/M3/M4) devices for Q4_K quantized models.

### Build System

| Commit | Description |
|--------|-------------|
| `fa0465954` | **Fix macOS build with GGML_BACKEND_DL=ON** - Resolves dynamic backend loading build issues |

---

## New Model Support

| Model | Commit | Description |
|-------|--------|-------------|
| **Qwen3 Next** | `ff55414c4` | Full support for Qwen3 Next architecture with SSM components |
| **Ministral3** | `cd3c11890` | Support for Mistral's Ministral3 model family |
| **LFM2-VL** | `2ba719519`, `6783b11fb` | Fixes for Liquid Foundation Model 2 vision-language |

---

## API Changes

### New Enums

```c
enum llama_model_meta_key {
    LLAMA_MODEL_META_KEY_SAMPLING_SEQUENCE,
    LLAMA_MODEL_META_KEY_SAMPLING_TOP_K,
    LLAMA_MODEL_META_KEY_SAMPLING_TOP_P,
    LLAMA_MODEL_META_KEY_SAMPLING_MIN_P,
    LLAMA_MODEL_META_KEY_SAMPLING_XTC_PROBABILITY,
    LLAMA_MODEL_META_KEY_SAMPLING_XTC_THRESHOLD,
    LLAMA_MODEL_META_KEY_SAMPLING_TEMP,
    LLAMA_MODEL_META_KEY_SAMPLING_PENALTY_LAST_N,
    LLAMA_MODEL_META_KEY_SAMPLING_PENALTY_REPEAT,
    LLAMA_MODEL_META_KEY_SAMPLING_MIROSTAT,
    LLAMA_MODEL_META_KEY_SAMPLING_MIROSTAT_TAU,
    LLAMA_MODEL_META_KEY_SAMPLING_MIROSTAT_ETA,
};
```

### New Functions

```c
// Get sampling metadata key name
const char * llama_model_meta_key_str(enum llama_model_meta_key key);
```

### GGML API Additions

- New `GGML_OP_TOP_K` operation
- New `ggml_top_k()` function for top-k element selection
- New `ggml_argsort_top_k()` function
- New `GGML_SCALE_FLAG_ANTIALIAS` flag

---

## Multimodal Updates

| Commit | Description |
|--------|-------------|
| `ecf74a841` | **mtmd_context_params::warmup option** - Adds warmup parameter for multimodal contexts |
| `7f8ef50cc` | **Fix CLIP nb calculation for Qwen3-VL** - Corrects vision encoding for Qwen3-VL models |
| `1d594c295` | **Fix MiniCPM-V resampler kq_scale** - Corrects attention scaling in MiniCPM vision |

---

## Performance Improvements

| Commit | Description |
|--------|-------------|
| `134e6940c` | **Skip output reordering for single token batches** - Reduces overhead for generation |
| `15d2b46b4` | **RPC: Cache and reuse compute graphs** - Improves RPC backend performance |
| `6eea66691` | **Avoid expand_forward for fusion** - Reduces graph expansion overhead |

---

## Bug Fixes

| Commit | Description |
|--------|-------------|
| `00c361fe5` | Fix llama arch implementation |
| `0874693b4` | Fix JSON schema with backslash in literals |
| `909072abc` | Fix CUDA UMA detection on discrete GPUs |
| `05872ac88` | Fix big-endian conversion in convert script |
| `5449367b2` | Fix chunks being too small with small matrix sizes |

---

## Risk Assessment

### Low Risk

- **API Changes:** All additions are backward compatible; no breaking changes
- **New Models:** New architectures don't affect existing model support
- **Metal Changes:** Focused addition (FA head size 48) with minimal surface area

### Medium Risk Areas

| Area | Risk | Mitigation |
|------|------|------------|
| ARM CPU repack changes | Performance regression possible on edge cases | Test Q4_K models thoroughly on device |
| Metal shader modifications | Potential GPU computation issues | Test various model sizes on iOS |
| Multimodal CLIP fixes | Vision model behavior changes | Verify Qwen3-VL and MiniCPM-V outputs |

### Recommended Testing

1. **Basic Inference Test**
   - Run inference with existing GGUF models
   - Verify token generation quality

2. **Performance Benchmark**
   - Compare Q4_K model speeds before/after
   - Test on both iPhone and iPad

3. **Vision Model Test**
   - Test Qwen3-VL if supported
   - Verify MiniCPM-V image understanding

4. **Metal Stress Test**
   - Test with large context sizes
   - Verify Flash Attention behavior

---

## Files Changed Summary

| Category | Files | Lines Changed |
|----------|-------|---------------|
| Metal Backend | 8 | +255, -37 |
| ARM CPU | 10 | +1,283, -110 |
| Core Library (src/) | 12 | +1,580, -47 |
| Common Library | 10 | +1,186, -1,000 |
| GGML Headers | 2 | +15, -6 |
| Multimodal | 5 | +39, -14 |

---

## Upgrade Checklist

- [ ] Clean previous build artifacts: `rm -rf build-apple build-ios-sim build-ios-device build-macos`
- [ ] Rebuild xcframework: `./build-xcframework-ios.sh`
- [ ] Run basic inference test
- [ ] Test Q4_K quantized models
- [ ] Test multimodal models (if applicable)
- [ ] Verify no performance regression
- [ ] Update app version notes if needed
