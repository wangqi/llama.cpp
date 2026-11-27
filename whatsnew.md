# llama.cpp Upgrade: b12632 → b12816 (user version v1.0.0 → v1.1.0)

**Date:** 2026-11-27
**Commits in range:** 188 upstream commits (`03831847818` our b12632 + patches → upstream `4c7344447`, b12816),
merge `db37b146d` plus our build-script copy line `b6b9e46f6`, on branch `tag-b12816`.
**Integration doc:** `helper/docs/llama_cpp.md` (speculative decoding: `helper/docs/llama_speculative.md`) ·
**Consumers:** `thirdparty/llamacpp_swift/Sources/`, `ai/AIChatModel.swift`, `ai/AIChatModelBase.swift`,
`libs/GGUFileStructure.swift`, `ai/tool/parser/`, `libs/audio/llamaaudio/`, `MacCommandLine/main.swift`

The previous file in this slot described b12361 → b12632; it is in git history at `03831847818`.

Upstream bumped llama.cpp to **1.1.0** (#33043) and ggml to **1.1.1** (ggml/1699, ggml/1703); our user version
follows to v1.1.0. No silent ship-blocker: the public headers only gained symbols (nothing our bridge calls was
removed or changed), there is no new `GGML_TYPE_*` id and no `LLAMA_SESSION_VERSION` bump. The one place a
ship-blocker could have hidden was found and fixed in P3: upstream moved the tensor-API matmul into the new
`kernels/mul_mm_tensor.metal` without the PrismML types, so on M5 / A19 and newer a Bonsai PQ2_0 / PTQ1_0 model
would have asked for a missing kernel (fixed in the merge, see §Conflicts). The most important items for the app
are on the Metal hot path: the graph is now **committed in chunks of 256 nodes** (#32947), which overlaps encoding
with GPU work and replaces upstream's reverted iPhone "8 command buffers" fix for the Bonsai GPU timeout
(#31815 → reverted #33050), and every model graph now goes through `ggml_mul_mat_ext` (quant scales, #28898).
Recurrent rollback for speculative decoding on Qwen 3.5 style delta-net models is now **replay**, not snapshots
(#31669) — it changes the recurrent state layout when `n_rs_seq > 0` (Mac speculative decoding only).

---

## New Features

### Performance (Metal / ARM64 / CPU)
- **Recurrent states read in place** instead of copied (#30139, `7b74cb92d`): measured about +6 % decode on
  Qwen3.5-35B-A3B (hybrid recurrent) on Metal in today's benchmarks; the change is in the shared recurrent input
  of `llama-graph.cpp` plus `delta-net-base` / RWKV6 / RWKV7, so the Qwen 3.5 family benefits on device too.
- **Inkling 2 Mini decode:** fewer ops in `mix()` / `control()` (#32948 `abca69200`), weighted sum with scores as
  src0 (#32855 `396ed2cce`), clip via `ggml_l2_norm` (#32940 `e683365a7`), indexed attention bias in the graph
  (#33053 `2d5b3ddde`, on the new `ggml_flash_attn_ext_add_indexed_bias`, #32740 / #33063; CPU accepts it #33016);
  `ignore_merges` vocab flag (#32799); NVFP4 pass-through (#33062).
- Metal: graph committed in chunks with one command-buffer thread (#32947 `52f6e0247`; only when `n_cb == 1`,
  no abort callback and no capture — our bridge sets none of these, so it applies); BF16 / F32 src1 for
  MUL_MAT / MUL_MAT_ID (#32677, which also split `mul_mm_tensor.metal` out of `mul_mm.metal`); **NVFP4 on Metal**
  (#32825); short mat-vec lanes spread over src1 rows (#33034); concat in vector chunks (#32824); top-k scratch
  only for short rows (#32942, large memory saving for long top-k); fused-group concurrency check now examines
  every fused node (#33175, correctness).
- CPU: no polling while the GPU computes when built without OpenMP (#32378 `d3c257a49`, our build has
  `GGML_OPENMP=OFF` — less CPU spin / battery on mixed CPU+GPU graphs); BF16 src1 in MUL_MAT (#32533);
  ARMv8.0 and SVE kernels (#32142, #30867) do not reach Apple CPUs.
- Tokenizer: BPE tokenization without string allocations (#32859).

### New Text Model Architectures
No new architecture in this range is an iOS candidate (≤ 4B):

| Model | PR | Notes |
|-------|-----|-------|
| Hy V4 Flash (`hy_v4_flash`, Tencent) | #32960 `c94333133` | Large MoE on the DSA iSWA cache, pooled sparse indexer; MTP and vision tower not covered yet. Not an iOS target. New mtmd encoder `hy-v4.cpp` comes with it (copy line added in `b6b9e46f6`). |
| RAON-1-mini-preview (`raon1`, KRAFTON) | #32176 `5a09582d8` | Hybrid KDA + MLA MoE with mHC, MXFP8 source weights, own chat template; size not stated in the commit, MoE → not an iOS target. |

Existing-arch additions: Inkling 2 Mini DFlash2 (#33073) and DSpark drafts (#32218) — speculative drafts our
bridge does not use (we run MTP and draft-simple).

### mtmd
- New encoder `hy-v4.cpp` (above). Qwen3-Omni audio tail fix (`qwen3a`, #32697) and shared audio FFT speed-up
  (#32815). mtmd public headers are unchanged.

### Correctness
- `llama_state_seq_set_data` with `LLAMA_STATE_SEQ_FLAGS_ON_DEVICE`: an unknown source seq now throws (caught →
  returns 0) instead of `GGML_ASSERT` (#30318). Our bridge does not use the seq-state API.
- Vocab: `metaspace_prepend_scheme` honoured for SPM (#31986, Nanbeige4.3) and "always" scheme for BPE (#33038).
- YaRN hparams (`rope.scaling.yarn_*`) now loaded for **every** arch in `llama_model_base::load_hparams` (#33032),
  previously read per arch (all four in grok, beta only in k2-horizon / laguna / mistral3). See risk L2 for how the bridge's defaults interact.
- Host weights shared by several nodes copy all experts (#32807); `SPLIT_MODE_TENSOR` refused for Hadamard-folded
  models (#33080, we never use tensor split).

### Jinja / chat (upstream `common/`, not compiled into our xcframework)
- Token-aware parsing (#29307), named tool choice (#31564), boolean JSON-schema `value` (#32908), Inkling parser
  / grammar (#32833), LFM2.5-VL template detection (#32843), RAON template. These do not reach the app (we render
  with swift-jinja and parse in `ai/tool/parser/`); nothing in them describes a model behaviour our parsers miss.

### Quantize (tool / `llama_model_quantize`, unused by the app)
- Target-size / target-bpw automatic type selection (#15550, the new `llama_model_quantize_params` fields).

---

## API Changes

### `include/llama.h` (+9, additions only)
- **Added** to `llama_model_quantize_params`: `activations`, `statistics`, `stats_schema`, `target_bpw`,
  `target_size`, `state_file`, `target_exclude`, `abort_callback`, `abort_callback_data` (#15550).
  Bridge / app: no caller of `llama_model_quantize*` (`grep -rn quantize` in `llamacpp_swift/Sources` and the app
  consumers: none). No fix.
- The old batch API stays deprecated (since b12632) and unchanged; no further removals.

### `ggml/include/ggml.h` (+71 / −7)
- **Added**: `ggml_needs_scale()`, `ggml_mul_mat_ext()`, `ggml_mul_mat_id_ext()` (weight / activation scales,
  #28898); `ggml_flash_attn_ext_has_indexed_bias()`, `ggml_flash_attn_ext_add_indexed_bias()` (#32740);
  `ggml_gated_delta_net_ext()` with replay inputs (#31669).
- **Deprecated**: `ggml_gated_delta_net(..., K)` (snapshot form) → `ggml_gated_delta_net_ext`.
- `ggml_type_traits`: comments only (`needs_scale` reserved for the next major).
- **No `GGML_TYPE_*` change:** NVFP4 = 40, Q1_0 = 41, Q2_0 = 42, F8_E4M3 = 43, PrismML `PQ2_0` = 142,
  `PTQ1_0` = 143, `GGML_TYPE_COUNT` = 144 — matches `libs/GGUFileStructure.swift`. No `llama_ftype` change.
- Bridge / app impact: none (the bridge calls no ggml graph builders; `grep` for every symbol above in
  `llamacpp_swift/Sources`, `ai/`, `libs/GGUFileStructure.swift`, `libs/audio/llamaaudio/`,
  `MacCommandLine/main.swift`: no hit).

### `ggml/include/gguf.h`, `tools/mtmd/mtmd.h`, `tools/mtmd/mtmd-helper.h`
- No change.

### State Save/Load Behavioral Changes
- `LLAMA_SESSION_VERSION` stays 11, `LLAMA_STATE_SEQ_VERSION` stays 4.
- **Recurrent memory layout changes when `n_rs_seq > 0`** on Qwen 3.5 / Qwen 3.5 MoE / qwen4exp / Clover-MoE and
  the KDA models (Kimi K3 / K3.1, GLM5-Next, BailingMoE3, RAON) (#31669): the conv row widens to
  `window + pending` and pending k / v / gate rows travel with the state, so a file saved with the old layout fails
  the row-size check (`mismatched r row size`). The row size is now written from `nb[1]` (equal to the old value
  when replay is off). With `n_rs_seq == 0` — every iOS context and every Mac context with speculative decoding
  off — the format is byte-identical to b12632.
- The app never writes session files today (`save_load_state` defaults to false and no config sets it), so
  **no session cache invalidation is required.** Latent bridge issue noted under L3.

---

## Risk Assessment — 7 risks (0 high, 3 medium, 4 low)

### MEDIUM: R1 — every model graph now calls `ggml_mul_mat_ext` (#28898, 177 model files)
**Problem:** the scale-aware matmul replaces `ggml_mul_mat` / `build_lora_mm` paths in every arch, on both our
backends; a GGUF with weight scales (FP8 / NVFP4 checkpoints) takes a new path. PrismML PQ2_0 / PTQ1_0 output was
proven identical to the b12632 binary in P3 (CPU, Metal, forced tensor path).
**Verify:** generate sweep vs P1 baseline (macOS 42/0/0, iOS 50/0/0: `run_model_tests.py --gguf-only --scan-local
--download-missing --force-build --compare /Volumes/ssd2t/modeltests/reports/20261127-104258.md`), compare
per-model rows; tool-call sweep vs macOS 32/0/10, iOS 31/0/10. **Status:** P10.

### MEDIUM: R2 — Metal graph committed in chunks of 256 nodes (#32947)
**Problem:** applies to every Metal graph we run (n_cb 1, no abort callback). A fusion cannot span chunks (handled
upstream); the iOS GPU watchdog behaviour that #31815 targeted is now covered only by this change.
**Verify:** the P10 generate sweep on Metal (Mac); on device, one long prefill (≥ 4k tokens) with a Bonsai PQ2_0
model and a 4B Q4_K_M on an iPhone, no `kIOGPUCommandBufferCallbackErrorTimeout` in the log. **Status:** Mac in
P10, device run needed.

### MEDIUM: R3 — speculative rollback on delta-net models is replay (#31669), Mac only
**Problem:** with MTP / draft speculative decoding on (`n_rs_seq > 0`, `LlamaSpeculative.swift` →
`lab_spec_context_params`), Qwen 3.5 targets now roll back by replaying pending tokens inside the GDN kernel
(new Metal kernel path, function-constant replay). Wrong replay = wrong text only after a partial rejection.
**Verify:** `cd testcases && ./run_tests.sh SpeculativeModeDecodingTests LlamaSpecDecoderTests` on the macOS host,
plus the `--spec` regression probe for the Qwen3.5-4B MTP pair (`helper/docs/llama_speculative.md` §1): output
identical to spec off at temp 0. **Status:** P10.

### MEDIUM→LOW: R4 — M5 / A19 tensor matmul path for PrismML types (P3 fix)
`mul_mm_tensor.metal` carries our 10 PQ2_0 / PTQ1_0 instantiations (P3). Proven on this pre-M5 Mac only with
`GGML_METAL_TENSOR_ENABLE=1` (outputs identical, `test-backend-ops` 272/272). **Verify:** a Bonsai PQ2_0 chat on
an A19 / M5 device. **Status:** needs device.

### LOW: L1 — tokenizer changes (#31986 SPM metaspace, #33038 BPE "always", #32859 BPE speed-up)
Only GGUFs carrying `metaspace_prepend_scheme` change tokens. **Verify:** generate + tool-call sweeps, Gemma
(SPM) rows identical to baseline.

### LOW: L2 — YaRN keys now loaded for every arch (#33032)
The bridge passes `yarn_ext_factor = -1` (inherit) but **pins** `yarn_attn_factor = 1.0`, `yarn_beta_fast = 32`,
`yarn_beta_slow = 1` (`ModelParams.swift`), so GGUF beta / attn values never apply (as before; mistral3 already
read them and was already overridden). Net change for us: `yarn_ext_factor` from the GGUF reaches more archs.
**Verify:** generate sweep rows of YaRN models (Qwen3 / Ministral 3) vs baseline.

### LOW: L3 — `LLaMa.load_state()` ignores the `llama_state_load_file` result (pre-existing, unreachable)
On a failed state read `n_token_count_out` is already set, so `nPast` would be restored over an empty cache.
Unreachable today (`save_load_state` never true). **Verify:** `grep -rn save_load_state config data data_macos`
stays empty; fix before ever enabling session files.

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| Source layout | CMake from tree | flat copy into `src/` + `src/CMakeLists.txt`; one new encoder copy line `tools/mtmd/models/hy-v4.cpp` (`b6b9e46f6`); copy list equals `tools/mtmd/models/` (74 files) |
| Metal library | separate tensor metallib (`mul_mm_tensor_tensor.air`, `fa_tensor.air`, now with `-DGGML_METAL_HAS_BF16`) when not embedding | `GGML_METAL_EMBED_LIBRARY=ON`; `kernels/mul_mm_tensor.metal` is in the embedded source list, no script change |
| Versions | ggml 1.1.1, llama 1.1.0 from CMake | read from the same CMake files, no script change |
| Upstream CMake drift | OpenCL Adreno binary-kernel options, kleidiai vendored flag, llguidance include path | n/a on Apple |

`build-xcframework.sh` itself is unchanged upstream. P5 rebuild: `xcframework successfully written out`, 0 `error:`
(`p5-xcframework.log`). **No structural changes.**

---

## Conflicts and fork patches (P3)

- `ggml/src/ggml-metal/kernels/mul_mm.metal`: took upstream (#32677 / #32825 new template signature and
  `_prec_bf16` / `_prec_f32` variants), re-added a `pq2_0` and `ptq1_0` instantiation (nl = 8) after each of the 8
  `q2_0` ones.
- `src/llama-quant.cpp`: fallback switch moved into `fallback_type_for()`; `PTQ1_0` / `PQ2_0` → `Q4_0` re-added
  there; our old `qk_k <= 32` default is absorbed by upstream's own check.
- `ggml/src/ggml-cuda/mmq-config-cdna.cuh` (not built): took upstream's retune (#32837), kept the 7 PQ2_0 rows
  retuned to mirror Q4_0.
- **Class 1 fix without a conflict:** new `kernels/mul_mm_tensor.metal` had no PrismML types; 10 instantiations
  added (5 sites × 2 types).
- Marker counts: all 14 OLD files unchanged; new markers in `mul_mm.metal` 8, `mul_mm_tensor.metal` 5,
  `llama-quant.cpp` 1, `mmq-config-cdna.cuh` 1. Type ids 142 / 143 / COUNT 144 intact; `ggml-metal-device.m` trap
  not triggered. `tools/app-bridge/*` untouched and compiles against the new headers.
- `prismml_merge.md` §4 should note: `mul_mm_tensor.metal` needs the PrismML instantiations; the quant fallback
  now lives in `fallback_type_for()`.

## PrismML triage (prismml/prism `d12ca13bb..43f31f400`, 120 new commits; none picked)
- `76b97aa60` + `29cddbe02` new type **PQ1_0** (binary, group 64); `2240b4078` / `4a7d55fa4` **PQ2_0M**. Decided:
  **not carried this run** — wait for a published model (a GGUF in either type would not load in our fork).
- `7097bb488` skip `prism.hadamard` rotation for unloaded tensors — check against upstream's own Hadamard runtime
  next run.
- `3f0a42e85` x86 VBMI PQ2_0; Metal PTQ1_0 / PQ2_0 verify kernels (`6f0266cd1`, `e0fe94351`, `78a71e84c`,
  `aea69dc3b`) — speculative-decoding speedups, mostly default off. Rest: CUDA, Vulkan, SYCL, CI, server.

---

## Opportunities (not built)

| Upstream feature | Consumer that would use it | Value |
|---|---|---|
| NVFP4 on Metal (#32825) + quant scales (#28898) | catalogue (`models_*.json`), `GGUFileStructure` already decodes NVFP4 | NVFP4 / FP8-scaled GGUFs now run on the GPU instead of falling back |
| Replay rollback (#31669) | Mac speculative decoding on Qwen 3.5 (`LlamaSpeculative.swift`) | smaller recurrent state (one state + pending rows instead of `1 + n_rs_seq` snapshots); re-measure MTP speed ratios |
| Bridge YaRN defaults → `-1` (inherit) for `yarn_attn_factor` / `yarn_beta_*` | `llamacpp_swift/Sources/swift/ModelParams.swift` | GGUF YaRN values (now loaded for every arch) would apply as in upstream CLI; needs a long-context regression first |
| `llama_state_load_file` result check in `LLaMa.load_state()` | bridge | prerequisite before ever enabling session files (L3) |
| `llama_batch_ext` + `llama_process` migration | bridge (`LLaMa.swift`, `LLMBase.swift`, `ParallelBatchManager.swift`) | clears the b12632 deprecation before the next major prune |
| PrismML PQ1_0 / PQ2_0M | fork + `GGUFileStructure` | only when a model is published |
| mtmd audio generation / ASR (from b12632) | `libs/audio/llamaaudio/` | unchanged opportunity; this range only fixes Qwen3-Omni audio tail and FFT speed |

## Verification
Baseline (old engine b12632, P1): generate macOS 42/0/0, iOS 50/0/0; tool-call macOS 32/0/10, iOS 31/0/10.
P3: `llama-completion` on LFM2.5-1.2B Q4_0 and Bonsai PQ2_0 / PTQ1_0 identical to the baseline binary on CPU,
Metal and forced tensor path; `test-backend-ops` PrismML 272/272. P5: xcframework built, 0 errors.
New engine regression, `GGUFTypeParityTests`, `ToolCallReplayTests`, speculative suites and app builds: pending
P10. Device-only: iPhone GPU watchdog (R2), M5 / A19 tensor path (R4), iOS memory limits.

## Action Items
1. **REQUIRED**: none before building (copy line in `b6b9e46f6`, P3 kernel instantiations in `db37b146d`).
2. **REQUIRED after building**: none — no session cache invalidation (app writes no session files).
3. **Recommended**: P10 sweeps vs the baseline above (R1, L1, L2), speculative suites + `--spec` probe (R3),
   device runs for R2 and R4.
