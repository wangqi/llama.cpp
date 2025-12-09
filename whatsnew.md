# llama.cpp Upgrade: b7222 -> b7332

**Upgrade Date:** December 9, 2025
**Total Commits:** 114 commits

## Summary

This upgrade brings significant improvements to Metal backend stability (crucial for iOS), new backend support for AMD CPUs, enhanced server multi-model capabilities, and several important bug fixes.

---

## New Features

### Metal Residency Sets Keep-Alive (#17766)

**Impact: HIGH for iOS**

Added a background heartbeat mechanism to keep Metal residency sets alive, preventing the OS from reclaiming GPU memory during long inference sessions.

- Configurable via `GGML_METAL_RESIDENCY_KEEP_ALIVE_S` environment variable
- Default keep-alive interval ensures GPU buffers remain resident
- Properly guarded by `@available(iOS 18.0, macOS 15.0, *)` check
- Fixes potential memory-related crashes on iOS devices during extended use

### ZenDNN Backend for AMD CPUs (#17690)

Added a new backend optimized for AMD EPYC and Ryzen processors:

- Accelerates matrix multiplication operations via AMD's LowOHA MatMul
- Supports FP32 and BF16 data types (BF16 best on Zen 4/5)
- Requires `GGML_ZENDNN=ON` CMake flag
- 1.1x-2x speedup on compatible AMD processors

### Server Router Mode - Multi-Model Support (#17470, #17704)

Enhanced server with multi-model management capabilities:

- **Router mode**: Single API endpoint managing multiple backend instances
- **Automatic model routing**: Requests routed based on requested model
- **LRU eviction**: Automatic model unloading with `--models-max` limit
- **Modality validation**: Ensures model supports conversation attachments
- New components: `server_models`, `server_prompt_checkpoint`

### Grammar Token Matching (#17816)

Extended grammar system to support token-level constraints:

- Match specific tokenizer tokens: `<[1000]>` or `<think>`
- Negate token matches: `!<[1000]>` or `!<think>`
- Useful for constraining outputs based on special tokens
- New grammar types: `LLAMA_GRETYPE_TOKEN`, `LLAMA_GRETYPE_TOKEN_NOT`

### Rnj-1 Model Support (#17811)

Added support for Rnj-1 model by refactoring Gemma3 architecture:

- Gemma3 now supports both SWA (sliding window attention) and non-SWA modes
- Added YARN rope scaling support for Gemma3 variants
- Added final logit softcapping support
- Template-based implementation: `llm_build_gemma3<true/false>`

### CUDA FILL Operation (#17851)

Added native CUDA kernel for FILL operation:

- Supports F32 and F16 data types
- Improves performance for models using fill operations
- Added inplace allocation support for FILL node

### WebGPU Unary Operations (#17764)

Extended WebGPU backend with comprehensive unary operation support:

- ABS, SGN, NEG, STEP, TANH, ELU, RELU, SIGMOID
- GELU, GELU_QUICK, SILU, HARDSWISH, HARDSIGMOID
- EXP, GELU_ERF
- Code refactoring and improved shader embedding

### CANN RoPE Enhancements (#17543)

Added support for advanced RoPE configurations on Huawei Ascend NPUs:

- Partial RoPE support (`rope_dims < ne0`)
- Vision mode (`GGML_ROPE_TYPE_VISION`)
- Improved handling of mRoPE (multi-RoPE) variants

---

## Bug Fixes

### CUDA FP16 Overflow in Flash Attention (#17875)

**Impact: HIGH**

Fixed FP16 overflow in tile Flash Attention kernel by adding offset to KQ_max computation (`FATTN_KQ_MAX_OFFSET = 0.6931f`). Affects all Flash Attention kernels: tile, vec, mma-f16, wmma-f16.

### Mach-O Version Number Fix (#17877)

**Impact: HIGH for iOS/macOS builds**

Fixed build issue where `LLAMA_BUILD_NUMBER` exceeded Mach-O 'current version' field limit (max 255 for micro version). Now explicitly sets `MACHO_CURRENT_VERSION 0`.

### Vulkan Top-K Bug (#17659)

Fixed bug in Vulkan top_k shader when there are ties in the input values, ensuring correct sorting behavior.

### HIP RDNA3 FP16/BF16 Matrix Multiplication (#17817)

Fixed matrix multiplication issues on AMD RDNA3 GPUs when using FP16 or BF16 data types.

### Metal Build Fix (#17799)

Fixed Metal build issues related to residency sets:

- Added proper `GGML_METAL_HAS_RESIDENCY_SETS` guards
- Fixed context destruction in tests
- Ensured compatibility with older OS versions

### Kimi-K2 Tool-Call Parsing (#17376)

Fixed parsing issues with Kimi-K2 model's tool call outputs.

### Vulkan Validation Extension (#17637)

Replaced deprecated `VK_EXT_validation_features` with modern equivalents.

### DeepSeek V1 MoE Model Size (#12652)

Corrected model type detection: DeepSeek V1 MoE is 16B (not 20B based on layer count).

---

## Optimizations

### CUDA SOLVE_TRI Optimization (#17703)

Optimized triangular solve operation using registers and FMAF (fused multiply-add), improving performance for models using this operation.

### Console Line Editing (#17836)

Enhanced console input with:

- Arrow key navigation (left/right)
- Home/End key support
- Ctrl+Left/Right word navigation
- Delete key support
- Bash-style history editing (Up/Down)
- UTF-8 support throughout

### Fill Node Inplace Allocation (#17870)

Enabled inplace allocation for FILL operations, reducing memory overhead.

### SYCL BF16 Support (#17855, #17780)

Added BFloat16 conversion support for Intel oneAPI SYCL backend.

---

## iOS/macOS Specific Changes

| Change                     | Impact | Notes                                      |
| -------------------------- | ------ | ------------------------------------------ |
| Metal Residency Keep-Alive | High   | Prevents GPU memory reclamation            |
| Mach-O Version Fix         | High   | Required for successful builds             |
| Residency Sets Guards      | Medium | iOS 18.0+ / macOS 15.0+ availability check |
| Metal Debug Output         | Low    | Node names now printed for debugging       |

---

## Risk Assessment

### High Risk

1. **Metal Residency Sets API**
   - Uses iOS 18.0+ APIs with availability guards
   - Risk: Older iOS versions may behave differently
   - Mitigation: Properly guarded with `@available` checks

2. **Gemma3 Architecture Refactoring**
   - Template-based implementation change
   - Risk: Potential behavior changes for existing Gemma3 models
   - Mitigation: Extensive testing recommended with Gemma3 variants

3. **Flash Attention FP16 Fix**
   - Changes numerical computation in attention kernels
   - Risk: Slight precision differences in outputs
   - Mitigation: Fix addresses overflow, should improve accuracy

### Medium Risk

1. **Grammar Token Matching**
   - New grammar parser features
   - Risk: Existing grammars unaffected, but parser complexity increased
   - Mitigation: New features are additive

2. **Server Router Mode**
   - Significant server architecture changes
   - Risk: Multi-model scenarios need thorough testing
   - Mitigation: Single-model mode unchanged

3. **CUDA FILL Operation**
   - New CUDA kernel
   - Risk: Potential edge cases in CUDA path
   - Mitigation: Falls back to CPU if issues occur

### Low Risk

1. **Console Improvements** - CLI-only changes
2. **ZenDNN Backend** - Only activates on AMD CPUs
3. **WebGPU Unary Ops** - WebGPU backend extension
4. **CANN RoPE** - Huawei NPU specific

---

## Recommended Testing

Before deploying to production:

1. **iOS Device Testing**
   - Test extended inference sessions (30+ minutes)
   - Monitor memory usage for GPU buffer retention
   - Verify Metal backend stability

2. **Model Compatibility**
   - Test all currently used GGUF models
   - Pay special attention to Gemma3 family models
   - Verify Flash Attention outputs match expectations

3. **Build Verification**
   - Confirm xcframework builds successfully
   - Verify framework symbols with `nm -gU`
   - Test on both simulator and device

4. **Performance Benchmarks**
   - Compare inference speed before/after upgrade
   - Monitor peak memory usage
   - Check for any performance regressions

---

## Migration Notes

No API breaking changes detected. The upgrade should be backward compatible.

Key considerations:

- If using custom Metal shaders, review residency set changes
- If targeting iOS < 18.0, verify availability guards work correctly
- Grammar files using new token syntax (`<[id]>`) require this version

---

## Build Commands

```bash
# Clean previous builds
rm -rf build-apple build-ios-sim build-ios-device build-macos

# Build xcframework
./build-xcframework-ios.sh

# Verify symbols
nm -gU build-apple/llama.xcframework/ios-arm64/llama.framework/llama | head -20
```