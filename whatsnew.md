# llama.cpp Upgrade: tag-b7988 → tag-b8089

**Upgrade date:** 2026-02-18
**Upstream commits included:** ~100 commits (PR #19280–#19693)
**Conflict files resolved:** `src/CMakeLists.txt` (1 file)

---

## New Vision Model Support

Two new multimodal vision encoders were added to `tools/mtmd/models/` and integrated into `src/clip-models/` and `build-xcframework-ios.sh`:

| Model | File | PR |
|-------|------|----|
| **Kimi-K2.5** | `kimik25.cpp` | #19170 |
| **Nemotron Nano 12B v2 VL** | `nemotron-v2-vl.cpp` | #19547 |

Both are now included in the iOS/macOS XCFramework build. The `build-xcframework-ios.sh` copy list and `sed` guard check were updated accordingly.

---

## Metal (Apple GPU) Improvements

These changes directly improve performance and correctness on all Apple devices:

- **Fix ACC op** (#19427): Corrected a wrong Metal ACC operation that could produce incorrect results. This is a correctness bug fix with direct impact on inference quality.
- **Improve concurrency** (#19555): Metal backend now exploits more GPU parallelism, improving throughput on Apple Silicon.
- **Support `GGML_OP_SET`** (#19548): New Metal kernel enabling additional graph operations.
- **`sum_rows` kernel upgraded to float4** (#19524): Vectorized kernel improves memory bandwidth utilization on Apple Silicon.
- **Extend `l2_norm` support for non-contiguous tensors** (#19502): Removes a previous limitation in norm computation.
- **Consolidate unary ops** (#19490): Internal cleanup; reduces Metal shader compile time and binary size.
- **Unary ops support non-contiguous `src0`** (#19511): Fixes incorrect fallback paths for certain graph topologies. Includes Metal F16 unary ops.

---

## LoRA API — Breaking Change

> WARNING: HIGH RISK if the Swift bridge or any caller uses the LoRA adapter API.

The LoRA API in `include/llama.h` was overhauled in PR #19280:

| Old API | New API |
|---------|---------|
| `llama_set_adapter_lora(ctx, adapter, scale)` | `llama_set_adapters_lora(ctx, adapters[], n_adapters, scales[])` |
| `llama_rm_adapter_lora(ctx, adapter)` | _(removed)_ |
| `llama_clear_adapter_lora(ctx)` | _(removed)_ |
| `llama_apply_adapter_cvec(...)` | `llama_set_adapter_cvec(...)` |

The new API sets all LoRA adapters in a single call (batch semantics) and only modifies the context if the adapter list actually changed. The old per-adapter add/remove API is gone.

**Action required:** Search for `llama_set_adapter_lora`, `llama_rm_adapter_lora`, `llama_clear_adapter_lora`, and `llama_apply_adapter_cvec` in the Swift bridge and calling code and update to the new signatures.

---

## Memory & KV Cache Fixes

- **Fix KV cache size for hybrid models** (#19559): Hybrid attention/SSM models (Falcon-H1, Gemma3n) could allocate the wrong KV cache size. Fixed. Directly affects inference correctness for these model architectures.
- **Graph: fix KQ mask, LoRA, cvec reuse checks** (#19644): Prevents incorrect reuse of graph inputs across runs, which could cause subtle inference errors on repeated calls.
- **Fix output reorder with backend sampling** (#19638): Corrects output ordering when backend-side sampling is used.

---

## New Text Models

| Model | PR |
|-------|----|
| Kimi-K2.5 (vision + text) | #19170 |
| Tiny Aya (AYA-23 series) | #19611 |
| GLM MoE DSA architecture | #19460 |
| JoyAI-LLM-Flash | #19651 |
| Kimi Linear conv state fix | #19531 |

---

## ARM CPU Performance (Apple M-series)

- **SVE in GEMM q4_K x q8_K 8x8 kernel** (#19132): Significant matrix multiplication speedup on ARM cores with SVE/SVE2. Apple M-series chips support SVE, so this benefits macOS native builds.
- **Fix non-LTO CPU feature detection** (#19609): Prevents LTO flags from leaking into CPU feature detection builds, avoiding potential SIGILL crashes when loading backends on certain configurations.

---

## Common & Build Fixes

- **`common`: replace deprecated `codecvt`** (#19517, #19565): Removes C++17 deprecated UTF-8 conversion; improves future compiler compatibility.
- **`common`: remove unused token utility functions** (#19506): API surface reduction.
- **`common`: inline small string helpers** (#19693): Minor compile-time optimization.
- **`ggml_is_view` added as public API** (#19539): New `ggml_is_view()` function exposed in the public GGML header.
- **`ggml`: fix binary broadcast for permuted `src1`** (#19484): Correctness fix for certain graph patterns.

---

## Build Script Updates (`build-xcframework-ios.sh`)

Three upstream fixes from the official `build-xcframework.sh` were ported to our custom script:

| Fix | PR | Change |
|-----|----|--------|
| Use `xcrun libtool` instead of bare `libtool` | #19605 | Ensures correct tool resolution under Xcode CLT environments |
| Use `xcrun -f vtool` instead of `command -v xcrun vtool` | #19605 | More reliable vtool availability check |
| Check only for `xcrun`; drop individual `libtool`/`dsymutil`/`xcodebuild` checks | #19605 | All these tools are resolved via `xcrun` anyway |

The `LLAMA_HTTPLIB` CMake option was removed upstream (#19623). Our script does not use this option so no change was needed.

---

## Risk Summary

| Area | Risk | Reason |
|------|------|--------|
| **LoRA API** | HIGH | Three functions removed, one renamed with new signature. Will cause compile error if used. |
| **Metal ACC op fix** | MEDIUM | Correctness fix — inference results may change slightly for models using the ACC op. |
| **KV cache fix for hybrid models** | MEDIUM | Affects Falcon-H1, Gemma3n models. Correctness improvement, may change memory allocation. |
| **New vision models** | LOW | Additive only. New files in `src/clip-models/`, no changes to existing encoders. |
| **Common API removals** | LOW | Removed functions were internal/unused. Unlikely to affect Swift bridge. |
| **Build script `xcrun libtool`** | LOW | Defensive fix. Works identically on standard Xcode CLT setups. |
| **ARM SVE GEMM kernel** | LOW | Opt-in via CPU feature detection; no-op if SVE not detected at runtime. |
