# llama.cpp Upgrade: b11854 → b12015 (user version v0.7.0 → v0.8.0)

**Date:** 2026-10-30
**Commits in range:** 155 upstream commits merged (`96c2b5317` our b11854 + PrismML follow-ups → `a34de8618`
b12015). Upstream released **v0.8.0** in this range (`8642541b2`, #31309), so the user version moves.
Merge commit `6856bc26d` on branch `tag-b12015`.
**Integration doc:** `helper/docs/llama_cpp.md` · **Consumers:** `thirdparty/llamacpp_swift/Sources/`,
`ai/AIChatModel.swift`, `ai/AIChatModelBase.swift`, `libs/GGUFileStructure.swift`, `ai/tool/parser/`

The previous file in this slot described b11716 → b11854; it is in git history at `96c2b5317`.

The round is dominated by Liquid AI's LFM3 family (text 1.7B, VL 2B, Audio 2B, Omni 2B, plus the d1
decision heads), all small enough for iOS, and by the new **FP8 E4M3** tensor type, whose id 43 collided
with nothing of ours but did conflict textually with the PrismML ids (keep-both, 6 conflicts). Two
forced consumer fixes (P7): (1) `GGUFType` needs `f8_e4m3 = 43`, otherwise the GGUF inspector truncates
on FP8 files and `GGUFTypeParityTests` fails; (2) **silent behaviour change**: a video frame is now
"video" only when it is both `mergeable` **and** `is_video` (#31283). Before b12015 `mergeable` alone
set the clip batch `is_video`, which our video path relied on, so without the new
`mtmd_bitmap_set_is_video(true)` call our frames would quietly drop to the still-image path (visible only
as worse video answers on Nemotron-Omni / LFM3-VL). Both fixed. Also silent: the build script copy list
needed the three LFM3 encoders, added in the merge commit.

---

## New Features

### New Vision / Audio Encoders
- **`lfm3vl.cpp`** — LFM3-VL vision encoder (conv2d blocks + embedding norm, `PROJECTOR_TYPE_LFM3VL`), with a
  dedicated video-frame preprocessor (`preprocess_video_frame`) and its own video template (#31252, #31283).
- **`lfm3a.cpp`** — LFM3-Audio audio-input encoder (`PROJECTOR_TYPE_LFM3A`), also used for live transcription
  through `mtmd_audio_stream` with an "asr" chat template (#31257).
- **`lfm3a-gen.cpp`** — LFM3-Audio speech generator: MaskGIT depthformer + codec decoder + ISTFT
  (`MTMD_GEN_AUDIO_TYPE_LFM3A`, `PROJECTOR_TYPE_LFM3A_GEN`); flash attention disabled for the depthformer.
- `paddleocr.cpp` and `deepseek4v.cpp` got small fixes; `nemotron-omni.cpp` now reads the stricter `is_video`.

### New Text Model Architectures
| Model | PR | Notes |
|-------|-----|-------|
| LFM3 (`lfm3`) — LiquidAI/LFM3-1.7B | #31252 | Hybrid conv + attention like LFM2, SWA; recurrent state rollback enabled. Also the trunk of LFM3-VL-2B / -Audio-2B / -Omni-2B |
| Limite (`limite`) — paradigma-inc/limite-1b-violetto | #29433 | 1B text model, new arch |
| LiquidAI d1-2B / d1-Omni-2B / d1-omni-600M | #31275 | **Not generative**: LFM3(-VL/-Omni) trunk with a decision head read at the last token, served by `POST /v1/systemone`. Text generation is rejected |
| MiniCPM-OCR-Jev | #30980 | **Not generative**: Qwen3.5-VL trunk with a value head that scores OCR text against an image |

Larger, not iOS targets: DeepSeek-V4.1-Flash (`deepseek41`, #31086), Mistral "le chonk" (`mistral-large4`,
#31376), plus GLM5-next QSA, MiniMax-M3, Qwen4-exp (merged tensors on convert, old GGUFs still load) and
DSV4 follow-ups.

### Metal / Apple Silicon
- Faster MXFP4 mat-vec; full threadgroups for unary / binary / GLU kernels (#31075), plus a compile fix for
  older Metal toolchains (#31360).
- New `GGML_OP_TOP_K_MASK` kernel (#31230); overflow fix for large matrix output offsets (#30072);
  `mul_mm_id` amax rescaling removed (#30924).
- FP8 E4M3 supported on CPU and in Metal `supports_op` (#27512).
- KV-cache rotation constants kept on the GPU (#31073).

### Runtime / Tokenizer / Jinja
- DRY sampler: sequence breakers capped at 64 distinct entries (`LLAMA_DRY_MAX_SEQ_BREAKERS`), duplicates
  ignored — fixes CPU/memory exhaustion from a large breaker list (#31035).
- Grammar: recursion depth limit, now mandatory (#29755, #31153); chat JSON nested > 128 levels rejected (#30242).
- Jinja `select` / `selectattr` argument handling fixed (#31059); qwen3-coder reasoning/content split
  before template separators (#30557); reasoning-budget forcing ends on an end tag from the prompt (#31374).
  All in `common/` (server path) — the app renders templates in Swift and parses tool calls in `ai/tool/parser/`.
- Gemma 3n / Gemma 4: `token_embd` created before the tied output, saving ~216 MiB when input and output share
  a buffer type (CPU-only load) (#31394).
- Loader reuses the tensor staging buffer (#31321); `no_alloc` respected in all memory caches (#31087).
- Recurrent memory and the DSV4 cache refuse rollbacks past the snapshots of the last ubatch (#31368).
- Security: control-vector layer index and M-RoPE position loops bounded (#31336); truncated GGUF tensor data
  rejected in merge (#31044); RPC bounds checks.

### Server (not shipped in the app)
Realtime conversation + transcription + tool calls on `/v1/realtime` (#31269, #31287), reasoning effort
levels from the template, deferred tools, router memory margin, cpp-httplib 0.64.0, many WebUI changes.

---

## API Changes

### `include/llama.h`
- **Added** `LLAMA_FTYPE_MOSTLY_F8_E4M3 = 42` (PrismML ftypes 141/142/143 kept) — no bridge use. Do **not**
  add it to `GGUFType` (that enum holds ggml_type ids; pinned by `testFtypeIdsAreNotInTheTensorTypeEnum`).
- **Added** `#define LLAMA_DRY_MAX_SEQ_BREAKERS 64` — `GPT_SPM.swift` passes `params.drySequenceBreakers`
  (default 4 entries); more than 64 distinct ones are now ignored with a warning. No fix needed.

### `ggml/include/ggml.h`
- **Added** `GGML_TYPE_F8_E4M3 = 43`, `GGML_FTYPE_MOSTLY_F8_E4M3 = 29`, `ggml_fp8_e4m3_t`. `GGML_TYPE_COUNT`
  stays 144 (PrismML `PQ2_0` 142 / `PTQ1_0` 143). **Forced app fix:** `GGUFType.f8_e4m3 = 43` + description
  `"f8_e4m3"` in `libs/GGUFileStructure.swift`.
- (`ggml-backend.h`, outside the header list: new batched tensor copy API, #30881, used by context state I/O.
  No bridge use.)

### `ggml/include/gguf.h`, `tools/mtmd/clip.h`
No change.

### `tools/mtmd/mtmd.h`
- **Added** `mtmd_bitmap_set_is_video()`; video frames must be **both** mergeable and is_video
  (`is_video_frame()`), and that now drives `batch_f32.is_video` and `preprocess_video_frame`.
  **Forced bridge fix:** `LLaMa_MModal.createBitmapsFromVideo` now calls `mtmd_bitmap_set_is_video(bitmap, true)`
  after `mtmd_bitmap_set_mergeable`.
- **Added** `enum mtmd_video_template` (DEFAULT / LFM3 / LFM3_OMNI), `struct mtmd_video_config`,
  `mtmd_get_video_config()` (model-recommended fps / frame count / frame size) — opportunity.
- **Added** `mtmd_audio_stream_params.prompt / n_prompt`, `mtmd_audio_stream_is_seq_start()`; the
  `get_n_rows` semantics are now "rows whose text has been sampled". Bridge does not use audio streams.
- **Added** `MTMD_GEN_AUDIO_TYPE_LFM3A`, `mtmd_gen_inp.penalty_repeat / penalty_last_n`. Bridge does not use gen.

### `tools/mtmd/mtmd-helper.h`
- **Added** `mtmd_helper_get_capabilities(model, mctx)` → `mtmd_helper_cap` {chat, vision, audio, video,
  audio_stream, text_gen, audio_gen, audio_gen_spk_ref} (#31183).
- **Deprecated** `mtmd_helper_model_can_chat` and `mtmd_helper_support_video` (moved, still exported). Bridge
  uses neither.
- **Added** `mtmd_helper_video_init_params.no_soundtrack`, `mtmd_helper_gen_audio_inp.n_past / out_mode / voice`,
  `enum mtmd_helper_gen_audio_out_mode`, `mtmd_helper_gen_audio_next_is_text()`,
  `mtmd_helper_gen_audio_get_logit_bias()`, `mtmd_helper_audio_stream_get_logit_bias()`. No bridge use
  (the ffmpeg video helper is not in our xcframework).

### State Save/Load Behavioral Changes
`LLAMA_SESSION_VERSION` 11 and `LLAMA_STATE_SEQ_VERSION` 4 unchanged; no file-format change. Context state
I/O now copies tensors through the batched copy API (#30881) — implementation only. **No session-cache
invalidation needed.** Recurrent rollback bound (#31368) only matters with `n_rs_seq > 0` (speculative
drafts); the bridge leaves it at the default.

---

## Risk Assessment — 5 risks (0 high, 2 medium, 3 low)

### MEDIUM: Video frames silently lose video preprocessing
**Problem:** #31283 split `is_video` from `mergeable`; our frames set only `mergeable`. **Fix:** bridge now sets
`mtmd_bitmap_set_is_video(true)` per frame (`LLaMa_MModal.swift`). **Verify:**
`TEST_TIMEOUT_SECS=1200 ./run_tests.sh VideoFrameMtmdTests` and `GGUFVisionModelTests`. **Status:** P10.

### MEDIUM: PrismML ids beside F8_E4M3 (6 conflicts, keep-both)
**Problem:** any wrong slot in a type table gives fluent nonsense on Bonsai PQ2_0. P3 audited every table;
quick gate `Ternary-Bonsai-1.7B-PQ2_0` passed. **Verify:** P10 generate + tool-call rows for the Bonsai
PQ2_0 / PTQ1_0 models. **Status:** P10.

### LOW: `GGUFType` parity
Fixed (`f8_e4m3 = 43`). **Verify:** `./run_tests.sh GGUFTypeParityTests` (build, not `--no-build`).

### LOW: DRY breaker cap
A user list with > 64 distinct breakers is truncated with a log warning. **Verify:** none needed beyond the
regression sweep.

### LOW: Context state I/O rewritten on batched copies
Formats unchanged. **Verify:** the session-cache round trip in the P10 sweep / a chat reopen on device.

---

## New models for the catalogue (≤ 4B, candidates only — nothing downloaded)

| Model | Params | Candidate GGUF (Q4_K_M size) | Fit for our bridge |
|---|---|---|---|
| LFM3-1.7B | 1.7B | `LiquidAI/LFM3-1.7B-GGUF` `LFM3-1.7B-Q4_K_M.gguf` (~1.1 GB) | Text chat; natural successor to LFM2.5-1.2B |
| LFM3-VL-2B | ~2B | `LiquidAI/LFM3-VL-2B-GGUF` Q4_K_M (~1.1 GB) + `mmproj-LFM3-VL-2B-Q8_0.gguf` (~203 MB) / F16 379 MB | **Text + vision works with our mtmd bridge** (image bitmaps); video works as frames (no `<X.X seconds>` timestamps — those come only from the helper video path) |
| LFM3-Audio-2B | ~2B | `LiquidAI/LFM3-Audio-2B-GGUF` Q4_K_M (~1.1 GB) + mmproj Q8_0 (~304 MB) | mmproj holds an audio-input encoder + speech generator → our `projectorRole` classes it `.chatInput`, so audio-in chat should load; speech output needs `mtmd_helper_gen_audio` (not wired). Needs a test before cataloguing |
| LFM3-Omni-2B | ~2B | `LiquidAI/LFM3-Omni-2B-GGUF` Q4_K_M (~1.1 GB) + mmproj Q8_0 (~507 MB) | Vision + audio input in one mmproj; usable as text+vision(+audio) chat, speech out not wired |
| Limite 1B Violetto | 1B | community only: `yamura4/limite-1b-violetto-GGUF` `gguf/limite-1b-violetto-Q4_K_M.gguf` (~734 MB) | Text chat; no first-party GGUF |
| d1-2B / d1-Omni-2B / d1-omni-600M | 0.6–2B | `LiquidAI/d1-2B-GGUF` etc. | Not chat models (decision heads) — see Opportunities |
| MiniCPM-OCR-Jev | — | `openbmb/MiniCPM-OCR-Jev-gguf` | Not a chat model (OCR scorer) |

Larger new architectures (DeepSeek-V4.1-Flash, Mistral Large 4) are out of scope.

---

## PrismML triage
`prismml/prism` b87ca7225 → 9d9efeadb (2026-10-29): 37 new commits since the 2026-10-24 triage, 33 CUDA-only.
Uncarried total 165. None picked. Non-CUDA candidates, all deferred: 30bc02218 (x86 AVX-512/AVX2 PTQ1_0 vec_dot,
macOS x86_64 slice perf only), 63f16c408 (batched sched input uploads, touches ggml-backend.cpp), a15369706
(Metal empty async uploads, marked wip), ac9c5fec9 (server top-k host time). Metal carry verified intact.

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| mtmd | Built as `libmtmd` (CMake list gained `lfm3vl.cpp`, `lfm3a.cpp`, `lfm3a-gen.cpp`) | Sources copied into `src/clip-models/` (glob) and compiled into `libllama`; 3 copy lines added in `6856bc26d` |
| `build-xcframework.sh`, `src/CMakeLists.txt` | Unchanged | Unchanged |
| `ggml/CMakeLists.txt` | `GGML_VERSION_MINOR` 27 → 28; new `GGML_SYCL_SUPPORT_XMX` option | No action (SYCL not built) |

Copy list equals `tools/mtmd/models/*.cpp` (the `diff` check in `llamacpp-patterns.md` prints nothing).

## Conflicts and fork patches

| File | Resolution |
|---|---|
| `ggml/include/ggml.h` | `GGML_TYPE_F8_E4M3 = 43` + PrismML 142/143, COUNT 144 ("44..141 unused"); `GGML_FTYPE` 29 + 128/129 |
| `include/llama.h` | `LLAMA_FTYPE_MOSTLY_F8_E4M3 = 42` + PrismML 141/142/143 |
| `ggml/src/ggml-cpu/quants.h` | keep both declarations |
| `ggml/src/ggml-cuda/mmq.cuh` | PQ2_0 mma case `write_back_mma<..., prec_src1, false>` (new trailing arg) |
| `gguf-py/gguf/constants.py` | 3 hunks keep-both |
| `src/llama-model-loader.cpp` | 2 hunks keep-both |

Each PrismML block tagged `wangqi modified 2026-10-30` (8 markers). `wangqi modified` inventory in `src ggml
tools common` identical to `96c2b5317` (arch-fallback.h 1, cuda/common.cuh 1, cuda/mmvq.cu 1,
ggml-metal-ops.cpp 4, src/CMakeLists.txt 5, llama-vocab.cpp 1), block bodies byte-identical; nothing absorbed
or restored. PrismML mention counts per file unchanged.

## Dependencies
No dependency move (cpp-httplib 0.62.0 → 0.64.0 is server-only, not in the xcframework).

---

## Opportunities (not built)
| Upstream feature | Consumer that would use it | Value |
|---|---|---|
| `mtmd_get_video_config()` + `mtmd_video_template` | `LLaMa_MModal.createBitmapsFromVideo` | Sample fps / frame count / frame size the model wants (LFM3 wants 2 fps, frame pairs) instead of our fixed ≤512 px sampling |
| `mtmd_helper_get_capabilities()` | bridge `LLaMa_MModal` / `GGUFileStructure.projectorRole` | One authoritative chat/vision/audio/audio_gen answer after load, replacing metadata heuristics |
| LFM3-Audio / Omni speech output (`mtmd_helper_gen_audio` with `n_past`, `out_mode`, `voice`, text/audio interleave) | `SpeechHelper` / voice chat | On-device speech-to-speech with a 2B model |
| `mtmd_audio_stream` with an "asr" prompt + `audio_stream_get_logit_bias` | Meetings live ASR | LFM3-Audio live transcription |
| LiquidAI d1 decision heads (`/v1/systemone`) | `libs/decision/` (Laya) | Local typed-decision probabilities from a 600M–2B GGUF |
| `GGML_TYPE_F8_E4M3` | Catalogue | FP8 checkpoints (mostly large models; low iOS value) |
| `mtmd_gen_inp.penalty_repeat` | TTS gen path | Fewer repeated codes in generated speech |

---

## Verification (so far)

- Merge: 6 conflicts resolved keep-both; marker inventory identical; clang `-fsyntax-only` on 4 touched C/C++
  files 0 errors; gguf-py constants import ok.
- Quick gate `Ternary-Bonsai-1.7B-PQ2_0`: PASS.
- `build-xcframework-ios.sh`: running at the time of writing (`p5-xcframework.log`).
- `GGUFTypeParityTests`, `VideoFrameMtmdTests`, `GGUFVisionModelTests`, full regression sweep and tool-call
  probe: pending (P10). Device-only behaviour (iOS Metal, memory) not verifiable on macOS.

---

## Action Items

1. **REQUIRED**: xcframework build clean, with the `clip_graph_lfm3*` symbols present.
2. **REQUIRED**: app build with the bridge fix (`mtmd_bitmap_set_is_video` must exist in the new headers) and
   `GGUFTypeParityTests` with the new `f8_e4m3` case.
3. **Recommended**: `VideoFrameMtmdTests` + `GGUFVisionModelTests` (`TEST_TIMEOUT_SECS=1200`); full GGUF regression
   against the P1 baseline, looking at the Bonsai PQ2_0 / PTQ1_0 rows.
4. **Not needed**: session-cache invalidation (versions unchanged).
