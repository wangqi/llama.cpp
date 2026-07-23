# llama.cpp Upgrade: b9993 → b10091

**Date:** 2026-07-22
**Commits in range:** 109 upstream commits merged

---

## New Features

### New Vision Models
- No new vision encoder `.cpp` files were added in this range. All 33 encoders under `tools/mtmd/models/` remain covered by `build-xcframework-ios.sh` (no build-script patch required).

### New Text Model Architectures
| Model | PR | Notes |
|-------|-----|-------|
| Laguna XS.2 & M.1 | #25165 | Poolside code models; new `laguna` architecture, conversion path, and chat templates |
| DeepSeek V4 (hyper-connections) | #25585, #25325, #25588, #25702 | Fused hyper-connection ops, "write only used rows in state", `seq_rm` fix, and reduced graph splits complete the DSV4 residual-stream path |
| HunyuanVL | #25514 | Fixed XD-RoPE config handling during conversion |
| BitNet | #25769 | Conversion now accepts the `BitNetForCausalLM` architecture name |
| Hunyuan V3 | #25641 | Convert script supports split MTP (multi-token-prediction) export |

### Metal / Apple Silicon
- **Metal Q2_0 support (#25419):** the Q2_0 2-bit type — added CPU-only in b9993 — now has a Metal kernel, so 2-bit models run GPU-accelerated on Apple Silicon.
- **Metal snake activation fusion (#25459):** fuses `mul, sin, sqr, mul, add` into one kernel.
- **KleidiAI SME2 f32 kernel (#24414)** and **SME vs SME2 dispatch (#25478):** better ARM matmul kernel selection on newer Apple Silicon; warns once when a weight type has no KleidiAI kernel (#25701).

### Vision (mtmd)
- **align_corners for Qwen3-VL (#25781):** vision position-embedding interpolation now uses `align_corners`, improving spatial accuracy.
- **RAII non-causal attention (#25723):** safer set/reset of non-causal attention in mtmd.
- **Text-only slot save/restore with mtmd (#25076):** server can save/restore text slots even with a multimodal model loaded.

### Speculative Decoding
- Infer the speculative/draft type from draft-repo sidecars (#25989) and resolve a draft repo to its requested sidecar (#25955).
- Auto-download `dflash-` and `eagle3-` HF sidecars (#25811).
- DFlash: rotate injected K/V cache (#25823); fix crash with `draft-simple` (#25720).

### Quantization
- Allow manual tensor types together with `--pure` (#25716).
- Exclude the i32 `ffn_gate_tid2eid` routing table from quantization (#25787).

### Third-party / Engine
- ggml core bumped **0.16.0 → 0.17.0** (ggml/1568).
- BoringSSL vendored update to 0.20260713.0 (#25624).

---

## API Changes

### `include/llama.h`
- No changes.

### `ggml/include/ggml.h`
- **Added (additive only):**
  - New ops `GGML_OP_DSV4_HC_COMB`, `GGML_OP_DSV4_HC_PRE`, `GGML_OP_DSV4_HC_POST` and functions `ggml_dsv4_hc_comb`, `ggml_dsv4_hc_pre`, `ggml_dsv4_hc_post` (DeepSeek V4 hyper-connections).
  - Inner-dimension contiguity helpers `ggml_is_contiguous_to_1/2/3`.
- No signatures removed or changed; no impact on existing call sites.

### `ggml/include/gguf.h`, `tools/mtmd/mtmd.h`, `tools/mtmd/clip.h`
- No changes.

### State Save/Load Behavioral Changes
- `llama_dsv4: write only used rows in state` (#25325) touches DSV4 state serialization only; no format change affecting session cache files for existing models. No session-cache invalidation required.

---

## Risk Assessment

### LOW: Additive-only API surface
Only ggml.h changed among tracked headers, and every change is a new op/function. Existing bindings compile unchanged.

### LOW: PrismML Q1_0 patch is now upstream (custom patch retired)
The custom PrismML Q1_0 1-bit patch is **no longer in the tree** (zero `// PrismML` markers). It became redundant: upstream PR #21273 (Pasha Khosravi) added native `GGML_TYPE_Q1_0` (= the former PrismML `Q1_0_g128`, block 128) and dropped the block-32 variant. This landed at b9993, so the tree at b10091 relies on native Q1_0 with no forward-port needed. The only remaining `wangqi modified` marker is the unrelated Metal threadgroup-cap fix in `ggml-metal-ops.cpp`. See `helper/docs/llama_cpp_prism.md`. **Follow-up:** load-test a Bonsai GGUF to confirm the shipped file uses the block-128 layout (block-32 files would need re-quantization).

### LOW: No new vision encoders
Build script already lists all 33 `tools/mtmd/models/*.cpp` files; no `build-xcframework-ios.sh` edit needed.

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| clip-models flatten | N/A | Copies + CMake-patches all mtmd model `.cpp` into `src/clip-models/` |
| Custom patches | None | Metal threadgroup caps (PrismML Q1_0 patch retired: now native upstream) |

**No structural changes required this cycle.**

---

## Action Items

1. **REQUIRED**: Rebuild the xcframework — `thirdparty/llama.cpp/build-xcframework-ios.sh`.
2. **Recommended**: Smoke-test a Q2_0 model on-device to confirm the new Metal 2-bit path; verify a Qwen3-VL image prompt reads correctly (align_corners change).
3. **No session-cache invalidation needed.**
