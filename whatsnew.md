# llama.cpp Upgrade Notes (b6871 → b6910)

## Overview
- Pulled upstream commits `b6871..b6910` (June–July 2025) bringing ~40 changes spanning model coverage, CPU/GPU backends, conversion tooling, and server UX.
- Major focus areas: broader multimodal/model support, Vulkan/CUDA stability work, and new CPU execution paths that affect Apple Silicon/iOS builds.

## Key Changes Impacting iOS & On‑Device Workflows
- **ARM64 execution improvements** – Chunked matmul & flash-attention paths (`517b7170e`, `dcca0d3ab`) reduce peak memory and enable matmul‑id chunking on ARM64, benefitting iOS devices running large context windows.
- **Expanded multimodal models** – Added CogVLM (`bacddc049`) and Qwen3‑VL/Qwen3‑VL‑MoE families (`d261223d2`), plus Minimax M2 and Granite Hybrid nano variants (`0de0a0157`, `e58d58560`). These require the new `src/models/` subtree that is now linked in `src/CMakeLists.txt` and bundled by our XCFramework build.
- **MRoPE / KV persistence fixes** – M‑RoPE data now lives in KV cells (`e3af5563b`) and ASAN issues resolved (`3464bdac3`), ensuring repeatable decoding across suspend/resume cycles typical in iOS apps.
- **MTMD pipeline alignment** – Vision projector graph updates (Qwen3‑VL) and new gguf tensor enums feed into `tools/mtmd`, preserving our iOS multimodal path once `build-xcframework-ios.sh` stages `mtmd*` sources.
- **Server/client polish** – Request limits increased and sensitive logging removed (`16724b5b6`, `c22473b58`), indirectly helping when embedding the server binary for internal tooling.

## Other Notable Upstream Enhancements
- Vulkan stability sweep: descriptor, shared-memory, and accumulation fixes plus new fusions (`5d8bb900b`, `2e76e0136`, `2976b0374`, `052df28b0`, `b9ce94017`, `10fcc4129`, `bcf5bda6f`).
- CUDA backend tweaks: tensor core support for MMF, fastdiv usage, argsort fixes, and MOE kernels (`31c511a96`, `e41bcce8f`, `229bf6862`, `4146d6a1a`).
- ggml sync & Hexagon/OpenCL adjustments (`6d39015a7`, `13002a089`, `9984cbb61`) keep secondary platforms aligned.
- Conversion tooling refresh: updated Transformers requirements and Qwen3-series metadata handling (`ce18efeaf`, `d261223d2`).

## Risk Assessment
- **Overall risk: Medium-High.**
  - **Source tree explosion** – New `src/models/*.cpp` files (70+) and adjusted CMake inputs can break local scripts if stale copies remain (mitigated by our manual `copy_mtmd_files()` step).
  - **Behavioural churn in core graph** – Refactors in `llama-model.cpp` and KV handling may surface latent regression on Metal when combined with on-device quantizations—requires thorough functional smoke tests.
  - **Backend divergence** – Heavy Vulkan/CUDA work is mostly inert on iOS, but shared ggml abstractions touched by these patches could introduce unforeseen side-effects.
  - **Tooling dependencies** – Updated Python requirements mean rebuilds of gguf converters/generators must be done in consistent environments to avoid drift.

## Recommended Validation
- Re-run `thirdparty/llama.cpp/build-xcframework-ios.sh` and verify resulting framework exports MTMD and new model symbols.
- Exercise integration tests on an iOS simulator and at least one physical device focusing on large-context decoding and multimodal prompts (Qwen3‑VL, CogVLM).
- Regenerate representative gguf models with latest converter to confirm metadata (`general.architecture`) matches new expectations.
- Monitor app bundle size/perf shifts due to additional model backends; prune unused sources if necessary for release builds.
