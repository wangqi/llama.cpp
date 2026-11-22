# llama.cpp Upgrade: b12361 → b12632 (user version v1.0.0, unchanged)

**Date:** 2026-11-22
**Commits in range:** 266 upstream commits (`1dcf09a6a` our b12361 + patches → upstream `8d60873df`, b12632-4),
merge `13b06c4e3` plus our build fix `652bdc5f7`, on branch `tag-b12632`.
**Integration doc:** `helper/docs/llama_cpp.md` · **Consumers:** `thirdparty/llamacpp_swift/Sources/`,
`ai/AIChatModel.swift`, `ai/AIChatModelBase.swift`, `libs/GGUFileStructure.swift`, `ai/tool/parser/`,
`MacCommandLine/main.swift`

The previous file in this slot described b12015 → b12361; it is in git history at `1dcf09a6a`.

No silent ship-blocker this time. The merge had no textual conflicts; the two build-level gaps (14 new mtmd
encoder files not copied by `build-xcframework-ios.sh`, and the fork's shadowing `stb_image.h` copies staying on
v2.30) are fixed in `652bdc5f7`. The most important item for the integration is the **old batch API
deprecation** (#32306, `a06044b93`): `llama_decode`, `llama_encode`, `llama_batch_init`, `llama_batch_get_one`,
`llama_batch_free` and `struct llama_batch` are now `DEPRECATED` in favour of `llama_batch_ext` + `llama_process`.
They still work unchanged (`llama_decode` still calls `ctx->decode(batch)`), and nothing in our build treats
warnings as errors, so this is warnings only — but it announces a removal, probably at the next major prune (the
v1.0 prune #30565 removed every deprecated function). The bulk of the range is mtmd **audio generation** (8 new
TTS / music / full-duplex pipelines) and Vulkan/CUDA/SYCL/OpenCL/WebGPU work that does not reach Apple targets.
No new GGML type ids, no session-file format bump.

---

## New Features

### New mtmd encoders (`tools/mtmd/models/`, all 14 copied by `652bdc5f7`)
- **Vision:** `inkling2v.cpp` — Inkling 2 vision mmproj (#32217, `5c008e000`; image token placement fix #32294).
- **ASR:** `vibevoice-asr.cpp` — Microsoft VibeVoice-ASR, plus Higgs Audio v3 STT through the existing
  `whisper-enc.cpp` (#32329, `fc504f91d`; adds `mtmd_helper_get_media_meta`).
- **TTS:** `kokoro.cpp` (Kokoro-82M, G2P via `tts_dict` metadata + `mtmd_gen_audio_g2p`, #32413 `307e3f155`),
  `tiny-voca.cpp` (4.5M-param English TTS, CMUdict G2P, #32427 `3574df84b`), `zonos3-gen.cpp` /
  `zonos3-spkenc.cpp` (Zyphra ZONOS3 Mini, 800M / 100M active hybrid GDN MoE, voice clone + speed/loudness,
  #32020 `7ae3c6b23`), `voxcpm2-gen.cpp` / `voxcpm2-spkenc.cpp` (OpenBMB VoxCPM2, MiniCPM4 2B backbone, #32443
  `e4b620af1`), `fish-s2-gen.cpp` / `fish-s2-spkenc.cpp` (Fish Audio S2 Pro, Qwen3 4B backbone, voice clone,
  #32330 `a68c6136b`; the codec encoder now also serves Drama 3 cloning #32586 `34ff2e047`).
- **Music:** `yue2-gen.cpp` (YuE2-3B songs, 48 kHz stereo, #32369 `92192d31e`), `music3-gen.cpp` + `snake-vae.cpp`
  (MiniMax Music 3, 44.1 kHz stereo, #32435 `d92ea9bb1`).
- **Full duplex:** `moshi.cpp` — Kyutai Hibiki 1B / 2B realtime French→English speech translation, server
  realtime endpoint only (#32491, `c622f623f`).
- Existing pipelines: Qwen3-TTS ICL / reference mode with a transcript of the reference (#32438, `8c16babba`).
- Fixes: Parakeet rel-pos OOB (#32722), padding-only Whisper chunks skipped (#30403), Muse Glimmer embedding
  order (#32433), video pixel aspect ratio and rotation (#32267).

### New Text Model Architectures
| Model | PR | Notes |
|-------|-----|-------|
| Domyn Edge 4B (`domynedge`) | #32559 `55d323507` | 4B dense, mixed RoPE/NoPE layers with per-layer head sizes and SWA. iOS-size candidate; no public GGUF named in the commit. |
| Inkling 2 Mini (`inkling2_loop`) | #32694 `29b52d398` | Looped variant of Inkling 2 (shared loop slots, variable pass count). Size: unknown from the commit/converter; GGUF: unknown. |
| tiny-voca / kokoro / zonos3 / moshi (text side of TTS) | see above | Arch ids exist only as the backbone of an mtmd audio pipeline, not chat models. |

Larger, not iOS targets: Inkling 2 (`inkling2`, large NVFP4/MXFP8 MoE, 384 experts, #32217), Zaya1 8B (Zyphra,
#32071), MiniMax-M3.1 (#32301), DFlash2 Ripple-65B draft (#31904).

### Metal / ARM64 / CPU
- Metal: implicit GEMM CONV_2D (#28069), CONV_2D tensor API on M5 + split-K on all chips (#32542), fused
  MUL/ADD norm chain in either operand (#32300), `NSMakeRange` argument fix (#32602), LFM3 fusion workaround
  (#32296), mul_mv destination offset overflow (#32221, our PQ2_0/PTQ1_0 kernels already used that form).
- CPU: `mul_mat_one_chunk` memory access pattern (#30846), all threads for large GET_ROWS (#32514), BF16 ROPE and
  SET_ROWS (#32387), F8_E4M3 decode without `ldexpf` (#32477), IQ2 quantization cache (#31910).
- Gemma 4 multi-sequence prompt processing (#32342) — relevant to `ParallelBatchManager`.
- KV: `seq_rm` no longer scans every cell per sequence (#32380); k-pool layout prefix kept on sequence edits (#32137).

### Correctness / security
- `.scale` tensors applied automatically by the loader, fixes several models (#32669, 39 files; qwen3next #32393
  and qwen2vl #32241 scales are part of this line).
- GGUF: no invalid enum values from untrusted file data (#32641); grammar integer overflow (#32551); buffered
  I/O fallback on EIO (#32545); `PATH_MAX` removed (#32557); sticky NaN in recurrent state (#31913); encoder
  `n_ubatch < n_tokens` now returns -1 instead of aborting.
- Tensor staging buffer allocated from the CPU buffer type (#31438).

### Jinja / chat (upstream `common/`, not compiled into our xcframework)
- Jinja scope-chain lookup, `json sort_keys`, 4 missing features, lower render overhead (#32396, #32366, #32352, #32023).
- Parsers: Gemma 4 tool call inside an unclosed thought (#32232), Ministral-3 / Qwen3 tool calls inside
  reasoning (#32271), LFM2 undeclared tool calls (#32164), lazy grammar on bare JSON tool calls (#32497),
  Inkling parser (#32381). These do not reach the app (we render templates with swift-jinja and parse tool calls
  in `ai/tool/parser/`), but they document model behaviour our parsers must tolerate — see risk L2.

### Third-party
- `vendor/stb/stb_image.h` v2.30 → v2.32 (#32305); our `src/stb_image.h` and `src/stb/stb_image.h` synced byte
  for byte in `652bdc5f7`. cpp-httplib 0.67.0 (server only).

---

## API Changes

### `include/llama.h`
- **Deprecated**: `llama_batch` (struct), `llama_batch_get_one`, `llama_batch_init`, `llama_batch_free`,
  `llama_encode`, `llama_decode` (#32306). Replacements `llama_batch_ext_*` / `llama_process` already existed at
  b12361. Bridge callers: `LLaMa.swift` (13 `llama_decode`, 1 `llama_encode`, 4 `llama_batch_init`, 3
  `llama_batch_get_one`, 2 `llama_batch_free`), `LLMBase.swift` (4 decode), `GPT_SPM.swift` (2+2),
  `ParallelBatchManager.swift` (init/free), `ModelParams.swift` (1), `samples/LlamaProxy.swift`. Impact: Swift
  deprecation warnings only. Not a drop-in replacement (opaque handle, per-token add calls), so not migrated now.
- **Added**: `enum llama_dflash_variant` + `llama_model_dflash_variant()` (speculative drafts; unused by us).
- **Added**: `llama_model_meta_arr_n()` / `llama_model_meta_arr_str()` — string-array metadata, currently only
  `tokenizer.tts_dicts` / `tokenizer.tts_dict.*` (Kokoro G2P). Unused by us.

### `ggml/include/ggml.h`
- **Added**: `GGML_OP_SLEEP` (`ggml_sleep`, backend testing) and `GGML_OP_SIGMOID_BACK` (`ggml_sigmoid_back`),
  appended to `enum ggml_op`; `typedef ggml_tensor_t`. `enum ggml_ftype` marked deprecated for v2 (#32647).
  No `GGML_TYPE_*` change: `PQ2_0` = 142, `PTQ1_0` = 143, `GGML_TYPE_COUNT` = 144. Rest is whitespace.
  Bridge/app impact: none (`libs/GGUFileStructure.swift` decodes `ggml_type` and `LLAMA_FTYPE`, not `ggml_ftype`).

### `ggml/include/gguf.h`, `tools/mtmd/clip.h`
- No change (`clip.h` no longer exists since b12361).

### `tools/mtmd/mtmd.h` (additions only)
- `mtmd_bitmap_set_transcript()`; `mtmd_gen_audio_type` + 8 values; `mtmd_gen_audio_info.n_channels` (stereo
  output, `n_samples` now counts all channels); `mtmd_gen_audio_g2p()`; `mtmd_gen_state_get_ref_codes()`;
  `mtmd_gen_inp` gains `speed`, `backbone_state*`, `n_steps`, `cfg_scale`. Bridge: uses none of the gen API.

### `tools/mtmd/mtmd-helper.h`
- **Added**: `mtmd_helper_video_decoder` (application-supplied video decoder, #32188) and field
  `mtmd_helper_video_init_params.decoder`; `mtmd_helper_get_media_meta()`; `mtmd_helper_cap.audio_duplex`;
  gen-audio inputs `speaker_ref_text`, `speed`, `loudness`, `cfg_scale`, `flow_steps`;
  `mtmd_helper_gen_audio_max_frames()`. **Removed** internal C++ `mtmd_helper_video_src` (→
  `mtmd_helper_video_stream_info`).
- Bridge: `LLaMa_MModal.swift` builds the opt with `mtmd_helper_init_opt_default()`, which fills the new
  `decoder` field with NULL, so `mtmd_helper_bitmap_init_from_file` keeps its behaviour. Video frames are
  extracted by the bridge itself (mergeable + is_video flags), not through `mtmd_helper_video_*`. No fix needed.

### State Save/Load Behavioral Changes
- `LLAMA_SESSION_VERSION` stays 11 and `LLAMA_STATE_SEQ_VERSION` stays 4; `llama_state_*` signatures and the
  KV write/read paths are unchanged (the KV diff is `seq_rm` and a new relative-pos input only).
- **No session cache invalidation required.**

---

## Risk Assessment

### MEDIUM: R1 — `.scale` tensor handling moved into the loader (#32669)
**Problem:** 39 files changed how weight scale tensors are applied (previously per-model in graph code). A GGUF
that carries `.scale` tensors could change output if the new path differs for an arch we ship.
**Verify:** generate regression vs baseline 86/1/0 of 87 (`run_model_tests.py --gguf-only`), compare per-model
rows, not just totals. **Status:** P10.

### MEDIUM: R2 — CPU mul_mat chunk access pattern (#30846) and Metal fused norm chain (#32300)
**Problem:** hot-path kernel changes on both of our backends; PQ2_0/PTQ1_0 (nrows = 1) are unaffected per P3, but
standard quants run through them.
**Verify:** same generate sweep plus tool-call 30/0/12 of 42; one Bonsai/PrismML row must stay green.
**Status:** P10.

### LOW: L1 — old batch API deprecated
Warnings only today; plan the `llama_batch_ext` migration of the bridge before the next major prune.
**Verify:** the bridge builds (`xcodebuild ... -scheme AIAssistantMac`), grep the log for `error:` (deprecations
appear as `warning:`).

### LOW: L2 — upstream parser fixes describe model behaviour (Gemma 4 unclosed thought, Ministral-3 tool calls in reasoning)
Our Swift parsers find `<|tool_call>` / `[TOOL_CALLS]` anywhere in the text; the split of an unclosed thought is
ours (`StringUtil.extractThinkingFromText` needs a `</think>`). **Verify:** tool-call regression Gemma 4 and
Ministral rows vs baseline.

### LOW: L3 — stb_image v2.32 in mtmd image decode
**Verify:** OCR reading probe 6/0/4 of 10 vs baseline, plus a vision row in the generate sweep.

### LOW: L4 — encoder now returns -1 when `n_tokens > n_ubatch`
`LLaMa.swift` already logs and returns `[]` on a non-zero `llama_encode`. No action.

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| Source layout | CMake from tree | flat copy into `src/` + `src/CMakeLists.txt`; 14 new encoder copy lines added (`inkling2v`, `vibevoice-asr`, `fish-s2-gen`, `fish-s2-spkenc`, `kokoro`, `tiny-voca`, `music3-gen`, `yue2-gen`, `snake-vae`, `voxcpm2-gen`, `voxcpm2-spkenc`, `moshi`, `zonos3-gen`, `zonos3-spkenc`) in `652bdc5f7` |
| `stb_image.h` | `vendor/stb/` v2.32 | fork copies in `src/`, `src/stb/` synced to v2.32 |
| Video | `mtmd-helper-video-ffmpeg.cpp` under `MTMD_VIDEO` | not built (`-DLLAMA_SUBPROCESS=OFF`, file behind `#ifdef MTMD_VIDEO`) |
| Upstream CMake drift | `GGML_VULKAN_SPIRV_INCLUDE_DIR` option, OpenMP LLVM 22.1.8 installer (Windows), OHOS in kleidiai | n/a on Apple |

`build-xcframework.sh` itself is unchanged upstream in this range. **No structural changes.**

---

## Conflicts and fork patches

- Merge `13b06c4e3`: no textual conflicts.
- Patch audit: 20/20 `wangqi modified` markers in src/ggml/tools/common kept file by file (plus 8 in the build
  script, 3 in `constants.py`, 1 in `llama.h`); PrismML ids `PQ2_0` = 142, `PTQ1_0` = 143, COUNT 144 intact; no new
  upstream GGML type ids. The `ggml-metal-device.m` trap is untouched (`MUL_MAT` still calls
  `ggml_metal_op_mul_mat_use_fwht`). Upstream moves of non-fork code: dflash split into `src/models/dspark.cpp`
  (#32016), `llm_graph_params::hdmd` dropped (#32596).
- `src/stb_image.h`, `src/stb/stb_image.h`: v2.30 → v2.32 (`652bdc5f7`, follows 73f53317e #32305). No marker on
  purpose: the files carry no fork delta and must stay `cmp`-identical to `vendor/stb/stb_image.h`.

## PrismML triage (prismml/prism, 6 new commits since the last fetch; none picked)
- `d12ca13bb` pq2_0m 2-bit KV-cache type — feature, would take `GGML_TYPE_PQ2_0M` = 145 (COUNT 146). Skip.
- `802b53737` Arm NEON PQ2_0 repack with Q8_K GEMV/GEMM — CPU-path speedup, opportunity (not a fix).
- `3f035ba66` server `--checkpoint-every-nt` — server only. `0fca8678e`, `cec7baeff`, `549a184d7` — CI.
- None fixes PQ2_0/PTQ1_0/Hadamard correctness, so per `prismml_merge.md` §3 none is picked.

---

## Opportunities (not built)

| Upstream feature | Consumer that would use it | Value |
|---|---|---|
| mtmd TTS: Kokoro-82M, tiny-voca, ZONOS3 Mini, VoxCPM2, Fish S2 Pro, Qwen3-TTS ICL (`mtmd_helper_gen_audio_*`) | `libs/audio/` `SpeechHelper` (new speaker type) via a bridge wrapper in `LLaMa_MModal.swift` | GGUF on-device TTS on the same engine; Kokoro / tiny-voca are tiny |
| mtmd ASR: VibeVoice-ASR, Higgs v3 STT | `ASRHelper` | GGUF ASR on the llama engine |
| Music: YuE2-3B, MiniMax Music 3 | Media Studio music | local song generation (Mac-sized) |
| Full duplex Hibiki 1B/2B (`audio_duplex`) | voice chat | live speech translation |
| `mtmd_helper_video_decoder` (AVFoundation-backed) | `LLaMa_MModal.swift` video path | upstream fps sampling, timestamps and LFM3-Omni soundtrack instead of our own frame extraction |
| `mtmd_helper_get_media_meta` | attachment pre-checks | duration / size without decoding |
| `llama_batch_ext` + `llama_process` | bridge (`LLaMa.swift`, `LLMBase.swift`, `ParallelBatchManager.swift`) | clears the deprecation before removal |
| PrismML `802b53737` NEON PQ2_0 repack | CPU path of Bonsai models | faster PQ2_0 when not on Metal |

## Verification
Baseline (old engine b12361): generate 86/1/0 of 87, tool-call 30/0/12 of 42, OCR reading probe 6/0/4 of 10.
New engine: pending P10 (regression sweep on the macOS test host); app builds for iOS and macOS pending.
Device-only: Metal on A-series GPUs and iOS memory limits are not covered by the Mac sweep.

## Action Items
1. **REQUIRED**: none before building (build fix in `652bdc5f7`).
2. **REQUIRED after building**: none — no session cache invalidation.
3. **Recommended**: generate + tool-call + OCR sweeps vs the baseline above (R1, R2, L2, L3).
