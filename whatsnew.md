# What's New in `llama.cpp` (b6804 → b6871)

## Highlights for Mobile & Edge Deployments
- **Experimental Hexagon NPU backend** (`63d2fc46e`): introduces `ggml-hexagon`, enabling on-device acceleration on Snapdragon (v73/v75/v79/v81) Android devices with support for Q4\_0, Q8\_0, MXFP4, and FP32 quantizations. Bundles new build presets, adb tooling, and developer docs.
- **Stability polish for Hexagon sessions** (`8284efc35`): ensures `buffer.device` is initialised during session setup to avoid undefined behaviour when dispatching workloads to the NPU.
- **KV-cache memory alignment adjustments** (`85a7d8677`, `7a0e900e3`): removes over-padding and aligns context / buffer ordering, reducing wasted RAM on mobile-class devices.

## Additional Improvements Across the Stack
- **Backend coverage**: major CUDA, SYCL, HIP, Vulkan, and OpenCL upgrades (e.g. GEMV fusion, new ops, improved argsort, better FA handling) expand hardware support and performance.
- **Model & conversion tooling**: new model recipes (LightOnOCR-1B, BailingMoeV2, Jamba listing) plus safer converters for mmproj, MXFP4, GPT-OSS, mistral-common, and pre-quantized weights.
- **Runtime reliability**: fixes for mmap buffer leaks (`945501f5e`), interpolate edge cases (`10640e31a`), partial stop token streaming (`8cf6b42d4`), and pipeline fallback on allocation failure (`5a4ff43e7`).
- **User experience**: `chat` now understands LFM2 tool calls (`c053e18a6`); embeddings can emit raw vectors (`1c1409e13`); server prints memory breakdowns (`0bf47a1db`) and web UI gains new OpenAI-compatible model selection (`9b9201f65`).

## Risk Assessment
- **Overall risk: Medium-High (3 key areas)**
  1. **New Hexagon backend** – very large code drop touching build, runtime, and CI; marked experimental and may destabilise Android NPU paths.
  2. **Memory layout changes** – KV-cache padding/order updates could surface regressions on devices that relied on previous sizing assumptions.
  3. **Backend proliferation** – numerous CUDA/SYCL/HIP/Vulkan changes introduce opportunities for platform-specific regressions that require broad retesting.

Recommended follow-up: run the full mobile regression suite (iOS Metal + Android Vulkan/Hexagon), validate KV-cache sizing on constrained devices, and smoke-test chat tool calls end-to-end.
