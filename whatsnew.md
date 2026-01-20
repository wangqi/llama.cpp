# llama.cpp Upgrade: tag-b7703 → tag-b7783

**Upgrade Date:** January 20, 2026
**Commits:** 81 commits
**Platforms:** iOS 16.4+, macOS 13.3+

---

## Executive Summary

This upgrade from tag-b7703 to tag-b7783 brings **81 commits** with improvements to Metal performance, bug fixes for memory management and mmap handling, and a new adaptive-p sampler. **No new vision models** were added in this range, so our custom build script requires **no changes**.

### Key Highlights

✅ **Performance**: Metal Flash Attention optimized for MLA heads, improved KV-cache mask construction
✅ **Stability**: Fixed critical memory reservation and mmap direct-io bugs
✅ **API**: New adaptive-p sampler, deprecated llama_adapter_lora_free
✅ **MTMD**: Fixed ASR for LFM2.5-Audio-1.5B, cleaned up CLIP callback mechanism

---

## 1. iOS/macOS Platform Changes

### Metal Backend Improvements

| Commit | Description | Impact |
|--------|-------------|--------|
| `271191906` | **Metal: Enable FA for MLA heads** | ⚡ Performance improvement for multi-latent attention models |
| `388ce8224` | **ggml: Extend ggml_pool_1d + metal** | New pooling operation support for Metal |
| `7d587e554` | **ggml-metal: Fix header copying for embedded** | Build system fix (doesn't affect our build) |

**Performance Impact:** Flash Attention optimization for MLA (multi-latent attention) heads improves inference speed for models using this architecture pattern.

### Multimodal (MTMD/CLIP) Changes

| Commit | Description | Impact |
|--------|-------------|--------|
| `c945aaaef` | **mtmd: Fix ASR for LFM2.5-Audio-1.5B** | 🐛 Critical fix for audio speech recognition |
| `d98b54812` | **Restore clip's cb() to its rightful glory** | 🔧 Refactored CLIP callback mechanism, extracted common debug code |
| `e047f9ee9` | **mtmd: Fix use_non_causal being reported incorrectly** | 🐛 Fixed incorrect causal attention reporting |

**Vision Models Status:** ✅ No new vision models added. Our build script already includes all models:
- cogvlm, internvl, kimivl, llama4, llava, minicpmv, pixtral, qwen2vl, qwen3vl, siglip
- whisper-enc, conformer, glm4v, youtuvl, mobilenetv5

---

## 2. API Changes

### New APIs

#### llama_sampler_init_adaptive_p

**Added in:** `13f1e4a9c`

```c
LLAMA_API struct llama_sampler * llama_sampler_init_adaptive_p(
    float   target,    // Target probability (0.0 - 1.0)
    float   decay,     // EMA decay (0.0 - 0.99)
    uint32_t seed      // RNG seed
);
```

**Purpose:** Adaptive-p sampler maintains a configurable target probability over time using exponential moving average. Must be last in sampler chain (like mirostat).

**Recommendation:** Combine with min-p for best results: `min_p → adaptive_p`

### Deprecated APIs

#### llama_adapter_lora_free

**Deprecated in:** Tag range (exact commit not identified)

```c
LLAMA_API DEPRECATED(void llama_adapter_lora_free(struct llama_adapter_lora * adapter),
    "adapters are now freed together with the associated model");
```

**Migration:** Remove manual `llama_adapter_lora_free()` calls. Adapters are automatically freed when the model is deleted.

**Risk:** ⚠️ LOW - If our code doesn't call this function, no action needed.

---

## 3. Performance Optimizations

| Area | Commit | Description | Platform Impact |
|------|--------|-------------|-----------------|
| **KV-Cache** | `2fbde785b` | Optimize KQ mask construction | All platforms |
| **BLAS CPU** | `8cc0ba957` | Optimize ggml_vec_dot_bf16 for Power9 | Not applicable (iOS/macOS use Metal) |
| **Memory** | `be8e3d951` | Don't reserve scheduler for warmups | iOS/macOS ✅ |

**iOS/macOS Impact:** The KV-cache optimization and scheduler warmup improvement directly benefit our platforms.

---

## 4. Critical Bug Fixes

### Memory Management

| Commit | Issue | Fix | Severity |
|--------|-------|-----|----------|
| `18361c579` | **server: fix memory reservations in populate_token_probs** | Fixed incorrect memory allocation | 🔴 HIGH |
| `be8e3d951` | **context: do not reserve scheduler for warmups** | Reduced unnecessary memory allocation | 🟡 MEDIUM |
| `39173bcac` | **context: reserve new scheduler when graph topology changes** | Proper scheduler reallocation on graph changes | 🟡 MEDIUM |

**Impact on iOS/macOS:** Memory management improvements reduce peak memory usage and prevent potential crashes.

### File I/O

| Commit | Issue | Fix | Severity |
|--------|-------|-----|----------|
| `960e5e3b4` | **llama-mmap: fix direct-io loading fallback EOF exception** | Fixed crash when direct I/O fails and falls back to mmap | 🔴 HIGH |
| `287a33017` | **llama: Extend fallback, fix fileno for dio file** | Better fallback handling for direct I/O | 🟡 MEDIUM |

**Impact on iOS/macOS:** These fixes prevent crashes when loading models with certain file access patterns. Critical for reliability.

### Model Loading

| Commit | Issue | Fix | Severity |
|--------|-------|-----|----------|
| `0c3b7a9ef` | **model: fix qwen3next broken due to #18683** | Fixed Qwen3 model loading regression | 🟡 MEDIUM |
| `e4832e3ae` | **vocab: fix attribute overrides for harmony** | Fixed vocab loading for Harmony models | 🟢 LOW |

---

## 5. Template System Overhaul

| Commit | Description | Impact |
|--------|-------------|--------|
| `c15395f73` | **common: implement new jinja template engine** | Complete rewrite of template system | 🟡 MEDIUM |
| `959ecf7f2` | **jinja: fix undefined keys and attributes** | Bug fixes for new engine | 🟢 LOW |
| `bbcdac018` | **jinja: fix object item order** | Proper dictsort implementation | 🟢 LOW |

**Impact:** The new Jinja template engine replaces the old system. Should be transparent to iOS/macOS app usage.

---

## 6. Other Notable Changes

### LoRA System

| Commit | Description | Impact |
|--------|-------------|--------|
| `a7e6ddb8b` | **lora: make sure model keeps track of associated adapters** | Improved adapter lifecycle management | 🟢 LOW |

### Sampling

| Commit | Description | Impact |
|--------|-------------|--------|
| `a89002f07` | **ggml webgpu: support for backend sampling** | WebGPU backend sampling (not used on iOS/macOS) | N/A |

### Documentation

| Commit | Description |
|--------|-------------|
| `516a4ca9b` | **refactor: remove libcurl, use OpenSSL when available** |
| `f709c7a33` | **ci, tests: use cmake to download models and remove libcurl dependency** |

---

## 7. Build Script Comparison

### Official vs Custom Script Differences

Our custom `build-xcframework-ios.sh` has these **intentional** differences from the official `build-xcframework.sh`:

| Feature | Official Script | Our Custom Script | Reason |
|---------|----------------|-------------------|--------|
| **Optimization** | Default flags | `-O3 -fno-finite-math-only` | Better performance |
| **MTMD Files** | Not included | Copied via `copy_mtmd_files()` | Vision/audio model support |
| **Headers Exported** | Limited | Includes `clip.h`, `mtmd.h`, `mtmd-helper.h` | API access for multimodal |
| **Platforms** | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst | Focus on supported platforms |
| **CMakeLists Patch** | None | FRAGILE patch for vision models | Required for linking |

### ⚠️ FRAGILE: CMakeLists.txt Patch

Our script patches `src/CMakeLists.txt` to include vision model files. **This upgrade did NOT break the patch** because:

1. ✅ No new vision models were added (mobilenetv5 was the last one in b7703)
2. ✅ `mtmd-helper.cpp` still exists in CMakeLists.txt
3. ✅ Our sed pattern still works

### ⚠️ Build Script Fix Required

**Issue:** cpp-httplib replaced libcurl (commit `516a4ca9b`) and uses macOS-specific Security framework APIs that fail on iOS/Mac Catalyst builds.

**Fix Applied:** ✅ Replaced `-DLLAMA_CURL=OFF` with `-DLLAMA_HTTPLIB=OFF` in all build configurations (iOS sim, iOS device, macOS, Mac Catalyst arm64, Mac Catalyst x86_64).

**Error Before Fix:**
```
error: use of undeclared identifier 'SecTrustCopyAnchorCertificates'
error: use of undeclared identifier 'kSecFormatX509Cert'
```

**Action Required:** ✅ **COMPLETED** - Build script updated to disable cpp-httplib.

---

## 8. Risk Assessment

### High Risk ⚠️

| Risk | Mitigation | Status |
|------|------------|--------|
| **Memory allocation bugs** | Fixed in commits 18361c579, be8e3d951, 39173bcac | ✅ Resolved |
| **mmap/direct-io crashes** | Fixed in commits 960e5e3b4, 287a33017 | ✅ Resolved |

### Medium Risk ⚠️

| Risk | Mitigation | Status |
|------|------------|--------|
| **Graph topology changes** | Scheduler now reallocates when graph changes | ⚠️ Monitor performance |
| **Template system overhaul** | Extensive testing needed if app uses templates | ⚠️ Test chat templates |
| **API deprecation (lora_free)** | Check if we call llama_adapter_lora_free() | ✅ Verify usage |

### Low Risk ✅

| Risk | Mitigation | Status |
|------|------------|--------|
| **MTMD callback changes** | Refactoring, no API changes | ✅ Safe |
| **New sampler (adaptive-p)** | Additive API, no breaking changes | ✅ Safe |
| **Vision model changes** | No new models added | ✅ Safe |

---

## 9. Testing Recommendations

### Critical Tests

1. **Memory Stress Test**
   - Load multiple large models sequentially
   - Monitor peak memory usage (should be lower)
   - Verify no crashes during model switching

2. **File I/O Test**
   - Test model loading from various storage locations (iCloud, local, app bundle)
   - Verify no crashes with large GGUF files (>4GB)
   - Test both mmap and direct-io paths

3. **Multimodal Test**
   - Test vision models (llava, minicpmv, qwen2vl, etc.)
   - Test audio models (whisper-enc, conformer)
   - Verify CLIP encoding works correctly

4. **Sampler Test**
   - (Optional) Test new adaptive-p sampler
   - Verify existing sampling methods still work

### Performance Benchmarks

Run `llama-bench` equivalent to measure:
- **Prompt processing**: Should be same or faster (KV-cache optimization)
- **Token generation**: Should be faster (Metal FA optimization)
- **Memory usage**: Should be lower (scheduler optimization)

---

## 10. Upgrade Checklist

- [x] **Merge upstream commits** (b7703 → b7783)
- [x] **Verify vision models** - No new models, build script OK
- [ ] **Rebuild xcframework** - Run `./build-xcframework-ios.sh`
- [ ] **Check API usage** - Search codebase for `llama_adapter_lora_free`
- [ ] **Test model loading** - All GGUF models load without crashes
- [ ] **Test vision models** - CLIP encoding works
- [ ] **Test audio models** - Whisper/ASR works (LFM2.5 fix)
- [ ] **Memory profiling** - Verify lower peak memory
- [ ] **Performance benchmark** - Compare inference speed

---

## 11. References

- **Commit Range:** [tag-b7703...tag-b7783](https://github.com/ggml-org/llama.cpp/compare/tag-b7703...tag-b7783)
- **Total Commits:** 81
- **Official Build Script:** `thirdparty/llama.cpp/build-xcframework.sh`
- **Custom Build Script:** `thirdparty/llama.cpp/build-xcframework-ios.sh`
- **CLAUDE.md:** `thirdparty/llama.cpp/CLAUDE.md` (upgrade procedure)

---

## Appendix: Full Commit List (Key Changes)

```
d1e355648 CUDA: Replace init_offsets kernel with iterators in cub-based argsort
271191906 metal : enable FA for MLA heads
959ecf7f2 jinja : fix undefined keys and attributes and int/float as bool
18361c579 server: fix memory reservations in populate_token_probs
365a3e8c3 ggml : add ggml_build_forward_select
287a33017 llama : Extend fallback, fix fileno for dio file
bbcdac018 jinja : fix object item order (and properly implement dictsort)
d1b4757de opencl: fix q6_K mv for m=1
2fbde785b kv-cache : optimize KQ mask construction
388ce8224 ggml : extend ggml_pool_1d + metal
c945aaaef mtmd : Fix ASR for LFM2.5-Audio-1.5B
c15395f73 common : implement new jinja template engine
be8e3d951 context : do not reserve scheduler for warmups
13f1e4a9c llama : add adaptive-p sampler
39173bcac context : reserve new scheduler when graph topology changes
a7e6ddb8b lora: make sure model keep track of associated adapters
d98b54812 Restore clip's cb() to its rightful glory
516a4ca9b refactor : remove libcurl, use OpenSSL when available
7d587e554 ggml-metal: do not copy headers for embedded
960e5e3b4 llama-mmap: fix direct-io loading fallback EOF exception
e047f9ee9 mtmd: fix use_non_causal being reported incorrectly
```

---

**Generated:** 2026-01-20
**By:** Claude Code Upgrade Analysis
**For:** AIAssistant iOS/macOS App
