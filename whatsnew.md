# llama.cpp Upgrade: b7549 to b7610

**Upgrade Date:** 2026-01-02
**Commits:** ~62 commits

## New Features

### New Model Support
- **Youtu-VL Model** (#18479): Full support for Tencent's Youtu-VL multimodal model
  - New projector type: `PROJECTOR_TYPE_YOUTUVL`
  - Siglip2 vision encoder integration
  - Window attention with explicit layer indexes (irregular pattern support)
  - New tokenizer pre-type: `LLAMA_VOCAB_PRE_TYPE_YOUTU`

- **IQuestCoder** (#18524): Added conversion support for IQuestCoderForCausalLM

- **Modern-BERT Template Fix** (#18529): Removed iswa template parameter, simplified implementation

### Vision/Multimodal Enhancements
- New vision model file: `youtuvl.cpp` in `tools/mtmd/models/`
- New GGUF keys for vision models:
  - `clip.vision.wa_layer_indexes` - explicit full attention layer specification
  - `clip.vision.window_size` - configurable window size
- Enhanced window attention support for irregular layer patterns

### Metal Backend
- **COUNT_EQUAL Operation** (#18314): Added Metal implementation for count_equal op
- Updated SSM_CONV test configurations with new kernel sizes
- SOFT_MAX support for larger tensors (200001 elements)
- Removed BLAS support from Metal documentation (clarification)

## Bug Fixes

### Critical Fixes
- **CUDA Large Tensor Copy** (#18433): Fixed assertion failure `ggml_nbytes <= INT_MAX`
  - Changed data types from `int` to `int64_t` in copy kernels
  - Affects: `cpy_scalar`, `cpy_f32_q`, `cpy_q_f32`, and related functions
  - Prevents crashes when processing large tensors

- **DeepSeek2 Expert Weights Scale** (#18479): Made `expert_weights_scale` optional
  - `LLM_KV_EXPERT_WEIGHTS_SCALE` now has `optional=false` changed to `optional=true`
  - Fixes loading of non-MoE models using DeepSeek2 architecture

### Other Fixes
- **RPC Performance**: Use `unordered_map::reserve` and `emplace` for better performance (#18513)
- Fixed tied embeddings handling for output layer in DeepSeek2 models

## API Changes

### Potentially Breaking
- **ggml_backend_compare_graph_backend()**: Signature changed
  ```c
  // Old:
  ggml_backend_compare_graph_backend(..., struct ggml_tensor * test_node);

  // New:
  ggml_backend_compare_graph_backend(..., struct ggml_tensor const * const * test_nodes, size_t num_test_nodes);
  ```
  - Now accepts array of test nodes instead of single node

### New APIs
- `add_vision_wa_layer_indexes()` - Set explicit full attention layers
- `add_vision_window_size()` - Set window attention size

## Build System Changes

- Added `<filesystem>` include in some components
- SYCL CMakeLists.txt: Added newline at end of file (#18503)
- KleidiAI SVE kernel additions for ARM

---

## Risk Assessment

### HIGH RISK
1. **API Breaking Change**: `ggml_backend_compare_graph_backend()` signature change
   - **Impact**: If you use this API directly, code will fail to compile
   - **Mitigation**: Update call sites to pass array and count instead of single tensor

### MEDIUM RISK
1. **CUDA int64_t Changes**: Large tensor handling changes in CUDA copy operations
   - **Impact**: Should be backward compatible, but watch for edge cases
   - **Mitigation**: Test with large context models

2. **Vision Model Changes**: New YouTuVL projector type and window attention patterns
   - **Impact**: New models may not work without updated mmproj files
   - **Mitigation**: Regenerate mmproj GGUF files if using custom vision models

### LOW RISK
1. **Metal COUNT_EQUAL**: New operation that wasn't previously supported
   - **Impact**: Should be additive/backward compatible

2. **DeepSeek2 Architecture Changes**: Optional expert_weights_scale
   - **Impact**: Allows loading more models, backward compatible

---

## Build Script Updates Required

### Required Changes to `build-xcframework-ios.sh`

The custom build script needs to add the new `youtuvl.cpp` vision model file:

```bash
# In copy_mtmd_files() function, add after glm4v.cpp:
cp -fp "tools/mtmd/models/youtuvl.cpp" src/clip-models/

# Also update the sed patch section to include it:
clip-models/youtuvl.cpp\
```

### Comparison Summary: Official vs Custom Script

| Feature | Official | Custom | Action Needed |
|---------|----------|--------|---------------|
| youtuvl.cpp | Yes | **No** | **ADD** |
| conformer.cpp | Yes | Yes | OK |
| glm4v.cpp | Yes | Yes | OK |
| -O3 optimization | No | Yes | Keep custom |
| ggml-opt.h header | Yes | Yes | OK |
| visionOS/tvOS builds | Yes | Commented out | Keep as-is |
| clip.h/mtmd.h headers | No | Yes | Keep custom |

### Recommended Script Update

Add the following line to `copy_mtmd_files()` function after the `glm4v.cpp` copy:

```bash
# wangqi 2026-01-02: Added new vision encoder from b7610 upgrade
cp -fp "tools/mtmd/models/youtuvl.cpp" src/clip-models/
```

And update the sed patch to include it in the CMakeLists.txt patching section.

---

## Testing Recommendations

1. **Build Verification**
   - Run `./build-xcframework-ios.sh` after applying updates
   - Verify framework symbols: `nm -gU build-apple/llama.xcframework/ios-arm64/llama.framework/llama | grep clip`

2. **Runtime Testing**
   - Test existing vision models (LLaVA, Qwen2-VL) still work
   - Test large context inference (>32K tokens) to verify CUDA fixes don't affect Metal

3. **Model Compatibility**
   - Verify existing GGUF models load correctly
   - Test any DeepSeek-based models

## Files Changed (Key iOS-Relevant)

- `ggml/include/ggml-backend.h` - API change
- `ggml/src/ggml-metal/ggml-metal.m` - COUNT_EQUAL op
- `tools/mtmd/clip.cpp` - YouTuVL support
- `tools/mtmd/clip-impl.h` - New defines
- `tools/mtmd/clip-model.h` - New hparams field
- `tools/mtmd/models/youtuvl.cpp` - **NEW FILE**
- `src/llama-model.cpp` - Model loading changes
- `src/llama-vocab.cpp` - Youtu tokenizer
