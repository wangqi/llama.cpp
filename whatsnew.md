# llama.cpp Upgrade: b7332 -> HEAD (Post-b7402)

**Upgrade Date:** December 14, 2025
**Total Commits:** 67 commits merged from upstream

## Summary

This upgrade brings critical threadpool race condition fixes, Metal backend SSM kernel optimizations for Mamba models, significant vision model (CLIP) architecture refactoring, YaRN regression fixes, and a new CLI experience. The most impactful change for iOS is the threadpool API simplification which improves stability.

---

## New Features

### New CLI Experience (#17824)

A complete overhaul of the command-line interface:

- `tools/main` renamed to `tools/completion` (`llama-main` -> `llama-completion`)
- New `tools/cli` with `llama-cli` binary providing enhanced user experience
- Jinja templating enabled by default (#17911)
- Improved argument parsing with negated args support (`--no-*` flags)

**Impact for iOS:** No direct impact - CLI tools not used in iOS builds.

### Server Presets/Config for Multiple Models (#17859)

Enhanced server with configuration presets when using multiple models:

- Define model-specific configurations via presets
- Improved multi-model management workflow

### Attention Temperature Offset (#18025)

Added `f_attn_temp_offset` parameter to graph building:

- Allows fine-grained control over attention temperature
- Useful for specific model architectures requiring temperature adjustments

### DIAG Operation Support

Added GGML_OP_DIAG support across backends:

- CUDA implementation (#17873)
- Vulkan implementation (#17893)
- Useful for models requiring diagonal matrix operations

### Vulkan Multi-pass Softmax (#17892)

Extended Vulkan softmax to handle large number of columns:

- Multi-pass algorithm for better numerical stability
- Improved performance on large context models

---

## Bug Fixes

### Threadpool Race Condition Fix (#17748)

**Impact: HIGH for iOS**

Critical fix for race conditions when dynamically changing thread counts:

- Combined `n_graph` and `n_threads` into a single atomic update
- Replaced `n_threads_max` + `n_threads_cur` with unified `n_threads` field
- Fixes potential crashes during multi-graph inference scenarios
- Improves stability for apps that dynamically adjust thread counts

**API Change:**
```c
// OLD (removed)
threadpool->n_threads_max
threadpool->n_threads_cur

// NEW
threadpool->n_threads
```

### YaRN Regression Fix + Logic Consolidation (#18006)

**Impact: HIGH for extended context models**

Fixed regression in YaRN (Yet another RoPE extensioN) scaling:

- Corrected `yarn_attn_factor` calculation
- Moved YaRN initialization from graph building to context creation
- Consolidated `yarn_attn_factor_adjust` logic
- Fixes issues with models using YaRN for extended context (e.g., 128K+ context)

### Mistral3 Attention Factor Fix (#17945)

Fixed `attn_factor` computation for Mistral3 model graphs:

- Improved consistency across model implementations
- Fixes potential output quality issues with Mistral3 variants

### Output Buffer Reallocation Sync (#17974)

**Impact: MEDIUM**

Added synchronization before reallocating output buffer in `llama_context`:

- Prevents potential data races during buffer reallocation
- Improves stability during dynamic batch size changes

### Memory Allocator Fix (#17884)

Fixed reuse-parent logic for misaligned sizes in `ggml-alloc`:

- Corrects memory allocation edge cases
- Prevents potential memory corruption in specific scenarios

### Batch Sequence ID Ownership (#17915)

Fixed sequence ID ownership handling in batch processing:

- Corrects edge cases in multi-sequence inference
- Improves speculative decoding stability

### Vulkan Flash Attention Data Race (#17887)

Fixed data race and hang in scalar/cm1 flash attention:

- Resolves deadlock scenarios in Vulkan backend
- Improves reliability on Vulkan-supported devices

### CUDA MMA Kernel Overflow (#17939)

Fixed overflow in MMA kernel without stream-k:

- Prevents numerical issues in CUDA matrix operations
- Affects NVIDIA GPU inference quality

---

## Optimizations

### Metal SSM Kernel Improvements (#17876)

**Impact: HIGH for iOS (Mamba models)**

Significant optimizations for State Space Models on Metal:

- Added batched version of `ssm_conv` kernel
- Improved SSM_SCAN performance
- Better utilization of Metal compute units
- Enables faster Mamba/Mamba2 model inference on Apple devices

**New Metal Functions:**
- `ggml_metal_library_get_pipeline_ssm_conv_batched()`

### Vulkan Performance Improvements

Multiple Vulkan optimizations:

- Faster Q6_K matmul (#17813)
- Improved `mul_mat_vec_iq1_s` speed (#17874)
- Support for get_rows with i32 (#17941)
- Non-power-of-2 expert count in topk_moe (#17872)

### macOS Backtrace Improvements (#17869)

**Impact: LOW for iOS**

Added macOS-specific backtrace printing:

- Prevents terminal corruption during crash dumps
- Better debugging experience on Apple platforms

---

## Architecture Changes

### CLIP/Vision Model Refactoring (#17965)

**Impact: MEDIUM**

Major restructuring of vision model code:

- Moved model computational graphs into separate files
- New files: `clip-graph.h`, `clip-model.h`
- Model-specific implementations in `tools/mtmd/models/`:
  - `cogvlm.cpp`, `internvl.cpp`, `kimivl.cpp`
  - `llama4.cpp`, `llava.cpp`, `minicpmv.cpp`
  - `pixtral.cpp`, `qwen2vl.cpp`, `qwen3vl.cpp`
  - `siglip.cpp`, `whisper-enc.cpp`
- Explicitly forbids inclusion of private headers (#17946)

### RoPE Scaling Refactoring (#18013)

Consolidated rope scaling handling in model conversion:

- Unified `rope_parameters` handling
- Better support for various rope types (linear, yarn, longrope, llama3)
- Cleaner code structure in `convert_hf_to_gguf.py`

### Sampler/Grammar Logic Refactoring (#17937)

Significant changes to sampler architecture:

- Refactored `common_sampler` structure
- Grammar sampler integrated into chain
- Per-sequence sampler support via `common_init_result::sampler(seq_id)`
- Removed separate `grmr` field in favor of unified chain

---

## iOS/macOS Specific Changes

| Change | Impact | Notes |
|--------|--------|-------|
| Threadpool Race Fix | HIGH | Critical stability improvement |
| Metal SSM Improvements | HIGH | Mamba model performance |
| YaRN Regression Fix | HIGH | Extended context models |
| Output Buffer Sync | MEDIUM | Dynamic batch stability |
| macOS Backtrace | LOW | Debugging improvement |

---

## Breaking Changes

### Threadpool API Change

The threadpool structure has been simplified:

```c
// BEFORE
struct ggml_threadpool {
    int          n_threads_max;  // REMOVED
    atomic_int   n_threads_cur;  // REMOVED
    ...
};

// AFTER
struct ggml_threadpool {
    int          n_threads;      // Single unified field
    ...
};
```

**Migration:** If you have any code directly accessing threadpool internals, update `n_threads_max` and `n_threads_cur` references to `n_threads`.

### CLI Tool Renaming

- `llama-main` is now `llama-completion`
- New `llama-cli` provides enhanced experience
- Legacy binary names maintained for Docker compatibility (#17964)

### Multimodal Argument Changes

Some mmproj arguments renamed for consistency:
- `--no-mmproj` -> `--mmproj-auto` / `--no-mmproj-auto`
- `--no-mmproj-offload` -> `--mmproj-offload` / `--no-mmproj-offload`

---

## Risk Assessment

### High Risk

1. **Threadpool API Change**
   - Internal API change affects any code using threadpool directly
   - Risk: Build failures if accessing removed fields
   - Mitigation: Change is well-isolated; Swift bindings unlikely affected

2. **YaRN Logic Relocation**
   - YaRN initialization moved from graph build to context creation
   - Risk: Behavioral changes for YaRN-enabled models
   - Mitigation: Fixes regression, should improve accuracy

3. **Sampler Refactoring**
   - Significant changes to sampler chain architecture
   - Risk: Edge cases in grammar/sampling behavior
   - Mitigation: Extensive upstream testing; core logic unchanged

### Medium Risk

1. **CLIP Refactoring**
   - Major code reorganization for vision models
   - Risk: Potential regressions in multimodal inference
   - Mitigation: Functionality preserved; only structure changed

2. **Output Buffer Sync**
   - Added synchronization call before reallocation
   - Risk: Minor performance impact in dynamic scenarios
   - Mitigation: Improves stability; overhead minimal

3. **Metal SSM Changes**
   - New batched SSM kernels
   - Risk: Edge cases in Mamba model inference
   - Mitigation: Additive change; falls back gracefully

### Low Risk

1. **CLI Tool Renaming** - iOS builds unaffected
2. **Vulkan Improvements** - iOS uses Metal, not Vulkan
3. **CUDA Fixes** - iOS uses Metal, not CUDA
4. **macOS Backtrace** - Debugging only

---

## Recommended Testing

Before deploying to production:

1. **Thread Safety Testing**
   - Test with varying thread counts during inference
   - Verify no crashes under thread count changes
   - Test multi-graph scenarios if applicable

2. **Extended Context Models**
   - Test models with YaRN scaling (128K+ context)
   - Verify output quality matches expectations
   - Compare before/after upgrade outputs

3. **Mamba/SSM Models**
   - If using Mamba models, verify Metal SSM performance
   - Test batch inference scenarios
   - Monitor memory usage patterns

4. **Build Verification**
   - Confirm xcframework builds successfully
   - Verify framework symbols: `nm -gU build-apple/llama.xcframework/ios-arm64/llama.framework/llama | head -50`
   - Test on both simulator and device

5. **Vision Models (if used)**
   - Test multimodal inference with CLIP models
   - Verify image understanding quality
   - Check for any API compatibility issues

---

## Migration Notes

### For xcframework Build

No changes required to `build-xcframework-ios.sh`. The threadpool API change is internal to the C implementation and does not affect the public API.

### For Swift Integration

The public `llama.h` API remains unchanged. No Swift code modifications should be necessary.

### Conflict Resolution Applied

During this merge, one conflict was resolved:

**File:** `ggml/src/ggml-cpu/ggml-cpu.c` (line 3234)
**Resolution:** Updated to use new `n_threads` field while keeping warning log suppressed (local preference).

---

## Build Commands

```bash
# Clean previous builds
rm -rf build-apple build-ios-sim build-ios-device build-macos build-tvos-sim

# Build xcframework
./build-xcframework-ios.sh

# Verify symbols
nm -gU build-apple/llama.xcframework/ios-arm64/llama.framework/llama | head -30

# Check for threadpool symbols
nm -gU build-apple/llama.xcframework/ios-arm64/llama.framework/llama | grep threadpool
```

---

## Changelog Summary

| Category | Count | Key Changes |
|----------|-------|-------------|
| Bug Fixes | 12 | Threadpool race, YaRN regression, buffer sync |
| Optimizations | 8 | Metal SSM, Vulkan matmul, softmax |
| New Features | 5 | New CLI, presets, DIAG op, attn temp |
| Refactoring | 4 | CLIP, sampler, RoPE, threadpool |
| Documentation | 3 | Ops tables, Docker examples |
| CI/Build | 5 | CANN, RISC-V, MinGW fixes |
