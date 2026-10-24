# llama.cpp Upgrade: b11716 → b11854 (user version still v0.7.0)

**Date:** 2026-10-24
**Commits in range:** 144 upstream commits merged (`5a269f433` b11716 → `df9beac71` b11854), no new
upstream release tag. Merge commit `dd0253c51` on branch `tag-b11854`.
**Integration doc:** `helper/docs/llama_cpp.md` · **Consumers:** `thirdparty/llamacpp_swift/Sources/`,
`ai/AIChatModel.swift`, `ai/AIChatModelBase.swift`, `libs/GGUFileStructure.swift`, `ai/tool/parser/`

The previous file in this slot described v0.5.0 (b11222) → v0.7.0 (b11716); it is in git history at `5a269f433`.

A small, low-risk round. `include/llama.h` and `ggml/include/gguf.h` did not change at all, and no
symbol the bridge or app calls changed (P7: **zero forced fixes**). The one silent ship-blocker was
the iOS build script: upstream added two clip encoders (`sensenova-u1.cpp`, `soprano.cpp`) that
`clip.cpp` constructs, and our script copies encoders from an explicit list, so the xcframework would
have failed to link. Both copies were added in the merge commit. For the app, the useful changes are
MoE correctness fixes on Metal, image preprocessing that now matches transformers for LFM2-VL and the
Qwen-VL family (both in the catalogue), and faster vocabulary loading.

---

## New Features

### New Vision / Audio Encoders
Two new `tools/mtmd/models/*.cpp`, both added to `build-xcframework-ios.sh` (merge commit, tagged
`wangqi modified 2026-10-24`); after the edit the script's list matches `tools/mtmd/models/` exactly.

- `sensenova-u1.cpp` — SenseNova U1 vision encoder (#28919): patch embedding, GELU, adjacent-pair 2D
  RoPE, spatial downsampling and a dense projector, dynamic resolution. Pairs with the new
  `sensenova_u1` text architecture (U1.5-8B-MoT, understanding branch only; generation branches are
  rejected at conversion). Works through the normal image path, so it runs in the app with an imported
  GGUF, but at 8B it is a Mac model, not an iPhone one.
- `soprano.cpp` — Soprano TTS (#28607): Soprano-80M / Soprano-1.1-80M
  ([ekwek/Soprano-1.1-80M](https://huggingface.co/ekwek/Soprano-1.1-80M)), a Qwen3 backbone plus a
  Vocos decoder loaded as a gen-only mmproj (`clip_cap.has_gen_audio`, `mtmd_caps.gen_audio`), with a new
  `soprano` pre-tokenizer. **Not usable in the app**: it runs only through `mtmd_helper_gen_audio_*` /
  `mtmd_gen_audio_process`, which `llamacpp_swift` does not call (same as Qwen3-TTS and Pocket TTS).
  Compiled in so the link succeeds. Size would suit iOS if a generation path is ever wired (Opportunities).

Other mtmd changes:
- MiniCPM-OCR 1.0 (#30979, `openbmb/MiniCPM-OCR-1.0`): the MiniCPM-V 4.6 stack (qwen35 text, new
  `PROJECTOR_TYPE_MINICPM_OCR_1_0`, resize aligned to 28). No new `.cpp`; uses the existing minicpmv graph.
- LFM2-VL / LFM2.5-VL: tiles now stretch the image to the grid like `Lfm2VlImageProcessor` instead of
  letterboxing (#30261), tiling params read from GGUF metadata with the old constants as defaults
  (#30840), bilinear resize matches torchvision (#30943).
- Qwen-VL family (Qwen2/2.5/3-VL, dots.ocr, Exaone 4.5, …): smart_resize with no padding and Python
  rounding, matching the HF processor (#30736).
- Gemma 4 unified vision (`PROJECTOR_TYPE_GEMMA4UV`) uses GELU in its FFN (#30882). The E2B/E4B
  catalogue models use the other Gemma 4 projector and are unaffected.
- `mtmd_gen` now has a state object (#30902) and `build_inp_temporal_merge` moved to the base class (#31065).

### New Text Model Architectures
| Model | PR | Notes |
|-------|-----|-------|
| Flint (`FlintForCausalLM`, `RecursiveSI/flint`) | #30928 | Qwen2-MoE style: leading dense layers, shared experts, SWA pattern, NoPE layers. Size not stated upstream; check before cataloguing |
| SenseNova U1 / U1.5-8B-MoT | #28919 | 8B, dense understanding branch; Mac only |
| Nemotron-3 / 3.5-Embed 1B (`nvidia/Nemotron-3.5-Embed-1B-BF16`) | #30877, #30974 | Embedding model (mistral3 conversion), tokenization fix in #30974. Not a chat model |
| Nemotron: non-standard GQA, BPE tokenizer, `domyn-small` pre-type (`domyn/Domyn-Small-v1.0`) | #23343 | Size not stated upstream |
| H2O-Lightning-4B decision model (`h2oai/h2o-lightning-4b`) | #30361 | Classifier head on Qwen3.5-4B, not a generator; labels stored in GGUF (`llama_model_decision_label`) |
| Qwen3 bidirectional attention + `dense_2` output projection | #30607 | For Qwen3-based embedding models |

≤4B iOS-suitable generators added this round: **MiniCPM-OCR 1.0** (same stack as the catalogue's
MiniCPM-V-4.6-Q8_0) and possibly Flint and Domyn Small once their sizes are confirmed.

### Metal / Apple Silicon
- Fix: `moe_reduce` fusion race when dst aliases its inputs, which gave rare non-deterministic wrong
  results on MoE models (#30795).
- Fix: missing MoE `mul_mm` kernel variant for 9 experts per token (#30610 / #30699).
- Fix: memory leaks in the GPU capture path (#30890). Debug capture only.
- Faster argmax (two-pass partial reduction) and `pool_1d` (#30862).
- `MUL_MAT_ID` with id `-1` skips the slot and zeroes the row (#26631); `kernel_mul_mv_id` gained `ntg`.
  Our PQ2_0/PTQ1_0 id instantiations go through the `kernel_mul_mv_id_t` typedef and follow the change (P3).
- New `GGML_OP_TOP_K_MASK` fused op for top-k sparse attention (QSA, #30640); used by qwen4exp only.

### CPU
- NEON now uses the single-instruction fp32 → fp16 conversion instead of the portable software path
  (#30505). Affects the CPU path on iPhone and Apple Silicon Macs (F16 KV writes, CPU fallback).
- Vectorized silu/expf (#30791) is s390x only; kleidiai SVE kernels (#30808) do not apply to Apple.
- `ggml_time_us` overflow fix (#30969).

### Runtime / Tokenizer / Jinja
- Vocabulary loading optimized (#30679): faster model load. `gguf_kv_to_str` gained a defaulted
  `max_len` argument; `clip-impl.h` keeps its own 2-argument copy (separate TU via our unity-build exclusion).
- MoE down projection scaled per expert token instead of a global amax (#30861); `-sm tensor` denied
  for Mellum/Flint (#31081). Multi-GPU only.
- K-shift skipped when no layer uses RoPE (#30965 context; `llama-kv-cache.cpp`).
- Tekken and GPT-4o pre-tokenizer regexes gain `\p{M}` in lookaheads (#30845); `pixtral` pre-type now
  uses the Tekken regex rather than Llama 3 (#30960).
- New `tokenizer.ggml.clean_spaces` key (#30940) and per-pre-type defaults (lfm2, glm4/5, gemma4, exaone4,
  mellum2, minicpm5, …). Only `llama_detokenize` applies it; the bridge decodes with
  `llama_token_to_piece`, so output is unchanged.
- LoRA adapters on `prism.hadamard`-folded weights (#30910).
- Jinja: hexadecimal string escapes (#30929), `unique` filter (#30819), namespace attributes no longer
  shadowed by dict methods (#30941). `common/` chat code is not linked by the app; templates are
  rendered by the bridge's own path.
- Server: realtime transcription API, `POST /tts`, `--connect` (`LLAMA_CONNECT`, OFF by default) — not built for Apple.

---

## API Changes

### `include/llama.h`
- **No change.** `LLAMA_SESSION_VERSION` stays 11, `LLAMA_STATE_SEQ_VERSION` stays 4.
- `src/llama-ext.h` (not shipped in the xcframework headers): **Added** `llama_model_decision_label()`.

### `ggml/include/ggml.h`
- **Added**: `GGML_OP_TOP_K_MASK` (appended after `GGML_OP_DSV4_HC_POST`), `ggml_top_k_mask()`,
  `ggml_prec_get_acc()`, `ggml_prec_get_src()`.
- **Documented**: `ggml_add_id` / `ggml_mul_mat_id` id `-1` semantics.
- Types unchanged: `GGML_TYPE_COUNT` 144, `Q1_0`=41, `Q2_0`=42, `PQ2_0`=142, `PTQ1_0`=143.
  `GGUFTypeParityTests` PASS (4 tests).
- Bridge/app impact: none. Neither uses `ggml_op` values or these functions (`ggml_type` only in
  `libs/GGUFileStructure.swift`).

### `ggml/include/gguf.h`
- **No change.**

### `tools/mtmd/mtmd.h`, `mtmd-helper.h`, `clip.h`
- **Changed (experimental audio-gen API)**: `mtmd_gen_audio_process()` takes a new `mtmd_gen_state *`;
  `mtmd_gen_inp.seed`, `mtmd_gen_inp.state_data/state_size` and `mtmd_gen_out.state_data/state_size`
  removed. **Added** `mtmd_gen_state_init/free/reset`, `mtmd_gen_state_params(_default)`,
  `MTMD_GEN_AUDIO_TYPE_SOPRANO`, `mtmd_caps.gen_audio`.
- **Added**: `mtmd_audio_stream_get_n_rows()`, `mtmd_audio_stream_get_n_rows_flushed()`.
- **Changed (helper)**: `mtmd_helper_gen_audio_inp.stream` (new first field), `mtmd_helper_gen_audio_inp_default()`,
  `mtmd_helper_gen_audio_flush()`; C++ `gen_audio` wrapper constructor replaced by `init()`/`valid()`.
- **Changed (clip.h, internal)**: `clip_encode_params.seed/state_in/state_out` → `gen_state`;
  `clip_gen_state_init/free/reset`; `clip_cap.has_gen_audio`.
- Bridge/app impact: none. The bridge uses only `mtmd_init_from_file`, `mtmd_context_params_default`,
  `mtmd_helper_init_opt(_default)`, bitmap / chunk / tokenize / `mtmd_helper_eval_chunks`,
  `mtmd_support_audio` and `mtmd_helper_bitmap_init_from_file` — all unchanged.

### State Save/Load Behavioral Changes
None. Session and sequence-state versions are unchanged and `llama-context.cpp` / `llama-kv-cache.cpp`
state read/write code is untouched. Existing state files stay valid (the app does not enable
`save_load_state` anyway).

---

## Risk Assessment — 5 risks (0 high, 2 medium, 3 low)

### MEDIUM: iOS build script encoder list
**Problem:** Without the two new `cp` lines the xcframework fails to link (`clip_graph_sensenova_u1`,
`clip_graph_soprano` undefined).
**Mitigation:** Lines added in `dd0253c51`. **Verify:** the running `build-xcframework-ios.sh` finishes
with 0 errors, and `nm` on the `ios-arm64` slice shows `clip_graph_sensenova_u1` and `clip_graph_soprano`.

### MEDIUM: Vision preprocessing changed for catalogue models
**Problem:** LFM2-VL tiles now stretch rather than letterbox and resize like torchvision; Qwen-VL-family
smart_resize no longer pads and rounds half-even. The image token count and pixels fed to
LFM2.5-VL-450M/1.6B, Qwen3.5 0.8B/2B/4B (vision), dots.ocr, PaddleOCR-VL and Youtu-VL can change.
Should be closer to reference, but a drift in output is possible.
**Verify:** the full GGUF regression pass with vision models against `baseline-b11716.json`
(`./run_model_tests.py --gguf-only`), comparing the vision-mode replies of those models; no REGRESSED.

### LOW: Tekken / pixtral pre-tokenizer change
`\p{M}` lookaheads and `pixtral` → Tekken regex change token boundaries for combining marks. Affects
Ministral-3-3B (catalogue). **Verify:** Ministral-3-3B rows in the regression pass, plus one prompt with
decomposed accents (e.g. "café") reading back intact.

### LOW: `MUL_MAT_ID` id `-1` / `kernel_mul_mv_id` signature
Upstream changed the id kernel signature; PrismML id kernels follow via the typedef. **Verify:**
`Ternary-Bonsai-1.7B-PQ2_0` quick gate (PASS, coherent "Paris" reply) plus an MoE model
(`LFM2-8B-A1B-Q2_K`, `Huihui4-8B-A4B`) in the regression pass.

### LOW: `gguf_kv_to_str` default argument vs `clip-impl.h` copy
Two declarations, separate translation units through our unity-build exclusion in `src/CMakeLists.txt`.
**Verify:** the xcframework build has no redefinition / ambiguous-call error in `clip.cpp` or `llama-impl.cpp`.

---

## PrismML triage

155 commits in `prismml/prism` are not carried (143 on 2026-10-17). Full list:
`/Volumes/ssd2t/modeltests/thirdparty-upgrade/run-20261024/llama/p3-prism-new-annotated.txt`. None picked.
New since the last triage and touching code we carry:

- `67a9ab994` ggml-cpu repack: null repack-trait / read callbacks. In our tree `set_tensor` dereferences
  `tensor->extra` unguarded (`repack.cpp:5161`), but the repack buffer is only chosen for a weight whose
  op passes `supports_op`, which requires a non-null trait (`repack.cpp:5201`, `:5217`), so the null path
  is not reached through the normal loader. Our aarch64 repack covers Q1_0 only. Low priority.
- `23e863c7d` gguf legacy Q2_0 size hint for rows not divisible by 128. Our `gguf.cpp:833` check is a
  plain `assert`, compiled out in release. Optional, 3 lines.
- `420bed3d3` llama-quant refuses PQ2_0/PTQ1_0 output without `prism.hadamard` metadata — quantize only.
- Performance only (unchanged from last time): `6bfcd79a2`, `4fbda1292` (NEON vec_dot PQ2_0 / PTQ1_0),
  `5e9365f05`, `df7c49e84` (Metal PTQ1_0 mat-vec), `79971a480`, `445fa8209`, `b87ca7225`.

Upstream itself added `hexagon` Q1_0/Q2_0 hadamard ops (#30627), WebGPU Q2_0 (#30914) and LoRA on
hadamard-folded weights (#30910); none affect Apple builds. No new ggml types upstream.

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| mtmd | Built as `libmtmd` (CMake list gained `sensenova-u1.cpp`, `soprano.cpp`) | Sources copied into `src/` and compiled into `libllama` |
| New in this range | No change to `build-xcframework.sh`; `-D` flags unchanged | 2 encoder copies added (SenseNova U1 #28919, Soprano #28607) |
| New CMake option | `LLAMA_CONNECT` (OFF) | Not set; tools gate `LLAMA_BUILD_TOOLS` intact |
| New sources | `common/file-lock.cpp` (common only) | Not compiled; `common/` not linked |

No other structural changes. No new public headers in `include/` or `ggml/include/`; the one new mtmd
include (`../mtmd-audio.h` from `soprano.cpp`) resolves to the already-copied `src/mtmd-audio.h`.

---

## Conflicts and fork patches

Clean auto-merge, 0 conflicts, 427 files. `wangqi modified` marker inventory identical before and after
(6 files). The 19 files changed on both sides were audited line by line: every fork-added line present,
no fork-removed line returned. Kept: Metal cpy `nth` restore and FA `nsg` cap, vocab UINT32 token_type
patch, tools `LLAMA_BUILD_TOOLS` gate, commented-out ggml-cpu `cplan` warning. The ggml-metal-device
trap stays retired. Details: `handoff-P3.md` in the run folder.

## Dependencies
No dependency move (vendored LibreSSL 4.4.0, BoringSSL, cpp-httplib 0.62.0 are server/common only).

---

## Opportunities (not built)
| Upstream feature | Consumer that would use it | Value |
|---|---|---|
| `mtmd_helper_gen_audio` stream mode + `flush()`, `mtmd_gen_state` | `llamacpp_swift` + `SpeechHelper` | Would make Soprano 80M, Qwen3-TTS and Pocket TTS GGUFs actually speak in the app |
| `mtmd_caps.gen_audio` | GGUF import (`libs/GGUFileStructure.swift` / ModelManager) | Detect a TTS-only mmproj and stop offering it as a chat vision/audio projector |
| `mtmd_audio_stream_get_n_rows(_flushed)` | Live ASR (Meetings) | Completes the Voxtral Realtime streaming path |
| LoRA on `prism.hadamard` weights | `llamacpp_swift` LoRA / FineTune | Adapters on Bonsai models |

---

## Verification (so far)

- Merge: 0 conflicts; marker inventory identical; both-sides audit 0 missing.
- Quick gate `Ternary-Bonsai-1.7B-PQ2_0` (`--force-build`): 1 passed, 0 failed.
- `GGUFTypeParityTests`: PASSED (4 tests).
- `build-xcframework-ios.sh`: running at the time of writing (`p5-xcframework.log`).
- Full regression sweep and tool-call probe: pending (P10).

---

## Action Items

1. **REQUIRED**: xcframework build must finish clean and contain the two new `clip_graph_*` symbols.
2. **Recommended**: full GGUF regression against `baseline-b11716.json`, looking closely at LFM2.5-VL,
   Qwen3.5 vision, dots.ocr and Ministral-3 replies.
3. **Recommended**: no session-cache invalidation needed (versions unchanged).
