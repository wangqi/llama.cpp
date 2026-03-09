# llama.cpp Upgrade: b8185 → b8240

**Date:** 2026-03-08
**Commits in range:** 59 upstream commits merged

---

## New Features

### New Vision Models
No new vision encoder `.cpp` files were added in this range. All existing encoders (cogvlm, conformer, glm4v, internvl, kimik25, kimivl, llama4, llava, minicpmv, mobilenetv5, nemotron-v2-vl, paddleocr, pixtral, qwen2vl, qwen3vl, siglip, whisper-enc, youtuvl) are already included in `build-xcframework-ios.sh`.

### New Text Model Architectures
No entirely new model architectures were introduced. Model-level changes are internal fixes and optimizations.

---

## Key Changes

### Autoparser: Complete Refactoring of Structured Output Parser (#18675, #20177, #20171)
The structured output / tool-call parser was completely rewritten with a new PEG-based architecture:
- **True streaming**: Tool call arguments are now streamed token-by-token instead of buffered
- **Argument reshuffle**: Optional capability to reorder tool arguments to match schema order
- **Graceful incomplete output** (#20191): UTF-8 incomplete byte sequences at stream end handled correctly; `needs_more_input` signal instead of parse error
- Impact: Better tool-calling reliability in streaming mode; existing behavior unchanged for non-streaming use

### Context Checkpointing (#20087, #20132, #20232)
- New `n_ctx_checkpoints` parameter saves KV-cache snapshots every N tokens for faster prompt reuse
- **KV Cache fix**: M-RoPE checkpoints (used by Qwen3VL, Llama4, Qwen2VL) now restore correctly
- **Multimodal fix**: Checkpoints are no longer created immediately after an image/video chunk, preventing corrupt cache state when multimodal content appears mid-context
- iOS impact: Server-based workflows gain faster prompt caching; local inference unchanged

### MoE Expert Weight Scaling Refactor (#20235)
The `build_moe_ffn()` function had a redundant `scale_w: bool` parameter removed from all 25+ MoE model implementations (DeepSeek, Qwen3 MoE, Mistral, Phi3, DBRX, Grok, etc.). Behavior is now controlled purely by the `w_scale` float value. This is a source-level internal refactor; compiled output is equivalent.

### New API: `llama_model_init_from_user()` (include/llama.h)
Added a new model creation path that accepts a `gguf_context*` plus a callback `llama_model_set_tensor_data_t` to populate tensor data programmatically, enabling in-memory model construction without GGUF files. No impact on existing iOS usage.

### IQ Quantization Fixes (#19861)
Added missing `memset` calls and other correctness fixes in IQ quant kernels. Fixes potential NaN/garbage output for IQ2/IQ3 quantized models on edge-case inputs.

### GGUF Locale-Dependent Float Fix (#17331)
Fixed GGUF metadata float printing that produced wrong values in non-English locales (e.g., `1,5` instead of `1.5`). Improves cross-platform GGUF compatibility.

### New GGML Op: GATED_DELTA_NET (#19504)
Added `GGML_OP_GATED_DELTA_NET` for hybrid SSM+attention architectures (e.g., Falcon H1, Zamba2-style models). Metal backend support expected in a follow-on PR. Currently CPU-only.

### KDA Chunk Size = 16 (#19827)
`kda` (Knowledge-Distillation Attention) chunk size reduced to 16. Improves accuracy of KDA-variant models.

### Server: Preserve Anthropic Thinking Blocks (#20120)
When converting Anthropic API messages to internal format, `<thinking>` blocks are now preserved in the conversion pipeline. Prevents thinking-mode content from being silently dropped in multi-turn conversations on the server.

### CPU Performance: Skip Redundant ROPE Cache Updates (#20149)
CPU backend skips re-computing the ROPE frequency cache when context parameters have not changed between decode calls. Small latency improvement for CPU-only inference.

---

## API Changes

### `include/llama.h`
- **Added**: `llama_model_set_tensor_data_t` typedef (callback for tensor initialization)
- **Added**: `llama_model_init_from_user(metadata, set_tensor_data, set_tensor_data_ud, params)` — programmatic model creation API
- **Comment fixes only**: `indices` (was `indicies`), `probabilities` (was `probabilites`) — no ABI change

### `common/chat-peg-parser.h`
- **Changed**: `tagged_peg_parser::parse_and_extract()` second arg changed from `bool is_partial` to `common_peg_parse_flags extra_flags`
- **Changed**: `debug: bool` field replaced by `flags: common_peg_parse_flags`
- Impact on iOS app: none — `common/` is not part of the compiled XCFramework

### State Save/Load Behavioral Changes
No changes to `llama_state_save_file` / `llama_state_load_file`. Existing session cache files remain valid.

---

## Risk Assessment

### MEDIUM: Autoparser Architectural Rewrite
**Problem:** The complete rewrite of the PEG-based structured output parser changes internal state machines significantly. Streaming tool calls may behave differently in edge cases (partial JSON, multi-byte unicode in argument strings).
**Mitigation:** The new architecture is specifically designed to handle incomplete UTF-8 gracefully. Test tool-calling workflows end-to-end after rebuilding the framework. Existing non-streaming tool calls are unaffected.

### MEDIUM: Multimodal Checkpoint Timing Change
**Problem:** The fix that prevents checkpointing right after an image chunk (#20232) changes when KV snapshots are created. Any code relying on the (buggy) checkpoint timing may see different cache reuse patterns.
**Mitigation:** This is a correctness fix. Multimodal inference quality should improve; no code changes required in the Swift layer.

### LOW: MoE Scale_w Removal
Internal `build_moe_ffn()` signature change affects source compilation only. All MoE models (DeepSeek, Qwen3 MoE, Llama MoE, etc.) continue to work identically at runtime. No GGUF format changes.

### LOW: New GATED_DELTA_NET Op (Metal Not Yet Supported)
`GGML_OP_GATED_DELTA_NET` is CPU-only. Hybrid Falcon-H1-style models that use this op will fall back to CPU for that op on Metal. No crash; performance may be reduced on affected models.

### LOW: IQ Quant memset Fixes
Correctness fix. Models using IQ2/IQ3 quantization may produce slightly different (more correct) outputs. Existing downloaded GGUF files do not need to be re-downloaded.

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS, Mac Catalyst | iOS device, iOS simulator, macOS, Mac Catalyst |
| Signing | Not disabled | Code signing explicitly disabled (`CMAKE_CODE_SIGN_IDENTITY=""`) |
| C/CXX flags | Via `COMMON_C_FLAGS` variable | Per-build type (`CMAKE_C_FLAGS_RELEASE`, `CMAKE_CXX_FLAGS_RELEASE`) |
| Vision models | Not handled (no mtmd copy) | Full `copy_mtmd_files()` section + CMakeLists.txt patch |
| tvOS / visionOS | Included | Excluded (not needed for app targets) |

**Conclusion**: No changes required to `build-xcframework-ios.sh` for this upgrade. All 19 vision encoder files are already listed in the copy section and sed patch.
