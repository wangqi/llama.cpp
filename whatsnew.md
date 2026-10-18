# llama.cpp Upgrade: v0.5.0 (b11222) → v0.7.0 (b11716)

**Date:** 2026-10-17
**Commits in range:** 494 upstream commits merged (`a97cce86a` → `797de63b2`, b11716). Tag `v0.7.0`
is `620d2225c`; the merge target is 47 commits past it.
**Merge commit:** `5bd8aafd1` on `master`

The previous file in this slot described v0.4.1 → v0.5.0; it is in git history at `7de420c29`.

---

## New Features

### New Vision / Audio Encoders
Three new `tools/mtmd/models/*.cpp`, all added to `build-xcframework-ios.sh` and confirmed linked
(`clip_graph_*` symbols present in every slice):

- `muse-spark-audio.cpp` — Muse Spark Open audio encoder (#30616); uses the normal file-based audio path
- `nemotron-omni.cpp` — NVIDIA Nemotron Omni image encoder (`patch_embeddings` over pixels)
- `voxtral-rt.cpp` — Voxtral Realtime, with the new streaming `mtmd_audio_stream` API (#30629).
  **Not usable in the app**: `mtmd.cpp` routes `PROJECTOR_TYPE_VOXTRAL_RT` only through
  `mtmd_audio_stream`, and `llamacpp_swift` sends whole files through
  `mtmd_helper_bitmap_init_from_file`. Compiled in so the link succeeds; listed nowhere user-facing.

Also: Cohere2 vision (#30062), MiniCPM-V 4.7 (#29416), EmbeddingGemma 2 text+vision+audio (#30054),
LightOnOCR native resolution by default (#30600).

### New Text Model Architectures
| Model | PR | Notes |
|-------|-----|-------|
| Prism Bonsai 2 27B (`prism.hadamard` runtime) | #29600 | Upstream now owns the Hadamard fold runtime we carried; see Risk Assessment |
| GLM-5.3-Flash (GLM5-Next) | #27773 | `glm5-next.cpp` |
| K2 Horizon (dense + MoVA) | #29535 | `k2-horizon.cpp` |
| Kolibri-1 (Aleph Alpha) | #30673 | `kolibri1.cpp` |
| Beam (Reflection) | #30670 | `beam.cpp` |
| Muse Spark Open | #30616 | `muse-spark.cpp` |
| Maion Coder | — | `maion-coder.cpp` |
| LFM2.5-Encoder 350M / 230M | #29862 | Encoders |
| Decision models: Granite Switch Decision, vLLM Decision 2 / D3, clef, nimble, pplx-decider, LiquidAI d1-3B / d1-omni-600M | #30676, #30378, #30383, #29831, #29844, #30044, #30110, #30114 | Classifier heads, not generators |
| MTP for Mellum 2.1, Qwen3.8 Flash Next image embeds | #30612, #30397 | Speculative decoding is not enabled in our bridge |

### Metal / Apple Silicon
- Few-row MMA mat-mul, extended to every remaining `src0` type (#29869, #30065): faster small-batch decode.
- Tensor-API flash attention kernels for F16 KV (#29570) and BF16 (#30491); 128/96 head-dim FA kernels (#30209).
- Fixes: `mul_mat_id` with duplicate ids (#30373), `kernel_mul_mm` on very large tensors (#28748),
  excess threadgroup memory in quantized FA (#29340), MUL_MAT+ADD fusion when the residual is a MUL_MAT (#30100),
  temporary private transfer buffers now released (#29777).
- FWHT performance work (#29602); `mxfp4` mat-mul in bf16 math (#29770); left/circular `GGML_OP_PAD` (#29561).

### CPU
- AVX2 dot product for `Q2_0` (#30447), and AVX2 CPUs now use F16C (#30270). Affects the macOS x86_64 slice only.

### Memory / Runtime
- Experimental GPU cache for MoE experts kept in host memory (`moe_cache_size`, #29887, #30112). Off by default.
- Logits buffer skipped when the graph produces none (#30255); KV cache rotation metadata saved exactly (#28498).

### Tokenizer / Jinja
- PLaMo-3 tokenizer (`LLAMA_VOCAB_TYPE_PLAMO3`, #30045); opt-in NFC normalization for Unigram (#30390).
- Jinja: loop-variable scoping (#30533), `tojson` float/nan/inf now matches Python (#30362, #30483), faster context copies (#30562).
- Chat API refactor (#30210) and output parts (#30389). These are `common/` only and not linked by the app.

### Security
- GGUF: reject tensor sizes that wrap after padding (#26979); integer overflow fix in ggml/gguf (#29384).
- `ggml_acc` out-of-bounds write with a negative offset (#30135).

---

## API Changes

### `include/llama.h`
- **Changed**: `LLAMA_SESSION_VERSION` 10 → 11, `LLAMA_STATE_SEQ_VERSION` 3 → 4.
- **Added**: `llama_context_params.moe_cache_size`. The bridge uses `llama_context_default_params()`, so it stays zeroed (disabled).
- **Added**: `llama_get_causal_attn()`; perf counters `n_reallocs_same` / `n_reallocs_diff`; `LLAMA_VOCAB_TYPE_PLAMO3`.

### `tools/mtmd/mtmd.h`, `clip.h`
- **Added**: `mtmd_audio_stream_*` streaming audio API, `mtmd_support_audio_stream()`; `clip_enc_state_*`, `clip_get_image_max_tokens()`.
- **Changed**: `mtmd_get_memory_usage()` now returns `struct mtmd_memory_usage` instead of a `std::map`. Not called by the app or `llamacpp_swift`.

### `ggml/include/ggml.h`
- No breaking change. `GGML_TYPE_COUNT` is still 144 with our `PQ2_0` (142) / `PTQ1_0` (143); `GGUFTypeParityTests` 4/4.

### State Save/Load Behavioral Changes
State files written before this upgrade fail the version check and load nothing. In
`llamacpp_swift` `load_state()` ignores the return value and `tokens_count` stays 0, so the
context starts cold rather than crashing. The app never enables `save_load_state` (the
`ChatConfig` copy is commented out), so no user data is affected.

---

## Risk Assessment

### MEDIUM: The Hadamard runtime is now upstream's, not the PrismML picks
**Problem:** Upstream `a9d27ac69` (#29600, PrismML-authored) landed the same `prism.hadamard`
runtime we carried, refactored around `llama_model::hdmd`. The auto-merge left both copies in place,
including a doubled inverse rotation of the token embedding in `build_tok` with no conflict marker,
which would give fluent nonsense with no error.
**Mitigation:** All 8 runtime files taken from upstream whole (our side was pure Hadamard code);
`grep -rn 'hadamard_inverses\|hadamard_weight_blocks\|hadamard_memo' src/ conversion/` is empty.
`Ternary-Bonsai-2-27B-PQ2_0`, `-PTQ1_0` and `Bonsai-8B-Q1` pass the regression suite, and a clean
load proves the graph verifier accepted every fold. Gains `prism.hadamard.version 2` (tied output).
Resolution detail: `prismml_merge.md` §4, "The b11716 merge".

### LOW: `arch-fallback.h` x86 `q2_0` alias dropped
Upstream added a real AVX2 `ggml_vec_dot_q2_0_q8_0`; keeping our alias would be a duplicate symbol in
the macOS x86_64 slice. The slice links.

### LOW: CUDA sign-fused FWHT removed
`fwht.{cu,cuh}` and `ggml-cuda.cu` taken from upstream. Not built for any Apple target.

### LOW: Old llama state files invalidated
See State Save/Load above. No action required.

---

## PrismML triage

143 commits in `prismml/prism` are not carried (104 on 2026-09-27; the jump is mostly work upstream
absorbed with #29600). New since the last triage and relevant to our quant types, all performance
rather than correctness, so none picked per `prismml_merge.md` §3:

- `6bfcd79a2` NEON `vec_dot` for `PQ2_0` (#265), `4fbda1292` NEON + i8mm `vec_dot` for `PTQ1_0` (#290)
- `5e9365f05` Metal: five `PTQ1_0` rows per dense mat-vec (#225)
- `79971a480` Metal: SwiGLU folded into the signed FWHT (#287), `445fa8209` four GDN state rows per SIMD group (#292)

The NEON pair is the most valuable if CPU fallback speed on iPhone ever matters.

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| mtmd | Built as `libmtmd` | Sources copied into `src/` and compiled into `libllama` |
| New in this range | No change to `build-xcframework.sh` | 3 encoder copies added (above) |

---

## Verification

- `build-xcframework-ios.sh`: all four slices built, 0 errors. `PQ2_0`/`PTQ1_0` kernels,
  `load_hparams_hadamard` and the new encoders present in `ios-arm64` and both macOS archs.
- Model regression pass 1 vs `baseline-b11222.json` (84 models): **81 passed, 3 failed, 0 skipped, zero
  REGRESSED**. The 3 failures were all in the baseline: two Laya files (decision models, now excluded
  from `--scan-local`) and `Bonsai-27B-Q1_0`, which was a half-downloaded file (1.92 of 3.80 GB),
  re-fetched with matching SHA-256 and now PASS. Expected clean sweep: 82 / 0.
- Pass 2, tool-call probe (38 catalogue models): **25 passed, 0 failed, 13 skipped** (all skips are
  documented capability exemptions).
- `ToolCallReplayTests`: 2/3. The failure is `LFM2.5-VL-1.6B-Q4_0`, a first-time fixture whose correct
  JSON-array call inside `<|tool_call_start|>` the app's parser chain does not handle. App parser gap, not an engine regression.
- `GGUFTypeParityTests`: 4/4.

---

## Action Items

1. **Recommended**: `ToolCallParserChain` should accept the JSON-array form inside LFM tool-call tags.
2. **Recommended**: HunyuanOCR now loads (baseline: load failure) but its vision output is `,,,,,,,`; check by hand.
3. **Recommended**: save the next clean full sweep as `/Volumes/ssd2t/modeltests/baseline-b11716.json`.
