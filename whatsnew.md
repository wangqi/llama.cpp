# llama.cpp Upgrade: b12015 → b12361 (user version v0.8.0 → v1.0.0)

**Date:** 2026-11-13
**Commits in range:** 355 upstream commits (`ab9aa10a9` our b12015 → upstream b12361, tag
`v1.0.0-b12359-b1b770d`), merge `dc4c4b127` plus our `6a0797808` (miniaudio refresh), on branch `tag-b12361`.
Upstream released **v0.9.0** (`cd959bae9`, #31646) and **v1.0.0** (`4cda6227a`, #32190; ggml 1.0.0 `3bfde28f9`)
in this range, so the user version moves to v1.0.0.
**Integration doc:** `helper/docs/llama_cpp.md` · **Consumers:** `thirdparty/llamacpp_swift/Sources/`,
`ai/AIChatModel.swift`, `ai/AIChatModelBase.swift`, `libs/GGUFileStructure.swift`, `ai/tool/parser/`,
`MacCommandLine/main.swift`

The previous file in this slot described b11854 → b12015; it is in git history at `ab9aa10a9`.

The single most important item for us is upstream **#31865** (`7d7f02875`): `tools/mtmd/clip.cpp` and every
`clip*.h` are gone, folded into `mtmd-model` / `mtmd-graph` / `mtmd-impl.h` / new `mtmd-subcontext`. That was a
**silent ship-blocker** for our build (no merge marker; `src/CMakeLists.txt` and `build-xcframework-ios.sh` still
named `clip.cpp`), fixed in the merge commit. The second is the v1.0 **API prune** (#30565, `8ca5a8008`): every
`DEPRECATED` function in `llama.h` and `llama_context_params.defrag_thold` are removed. The bridge already used
the replacements; the only caller left was the `MacCommandLine` sample target, fixed in P7. No new GGML type
ids, no session-file format bump.

---

## New Features

### New Vision / Audio Encoders (`tools/mtmd/models/`)
- **`magevl.cpp`** — Microsoft Mage-VL-Cowork vision encoder (`PROJECTOR_TYPE_MAGEVL`, LLaVA-OneVision-2 layout,
  4:6:6 split rope over a Qwen3-4B text model) (#31819, `9f0e5098a`). Computer-use tuned VLM, 4.7B total.
- **`nemotron-transcribe.cpp`** — NVIDIA Nemotron 3.5 Transcribe audio encoder with a speaker FIFO (ASR +
  diarization) (#31471, `28b812d6c`). Only published at 30B-A4B.
- **`chatterbox-gen.cpp`, `chatterbox-spkenc.cpp`** — Resemble AI Chatterbox TTS (Turbo English, Multilingual V3,
  23 languages): speech generator + speaker encoder for voice cloning, built-in voices, classifier-free guidance
  read from GGUF metadata (#30993 `b97627967`, #31698 `b1c902f02`). New `MTMD_GEN_AUDIO_TYPE_CHATTERBOX`.
- **`drama3-gen.cpp`** — Fish Audio Drama 3 TTS (Qwen3.5-MoE 35B-A3B backbone + codebook tower + Fish DAC,
  44.1 kHz) (#31613, `5851a8e50`). `MTMD_GEN_AUDIO_TYPE_DRAMA3`.
- Existing encoders: Kimi K3.1 vision (#31980, in `kimivl`), **Gemma 4 input audio stream** (#31965, realtime
  stream path), **Gemma 4 images resized without letterbox padding** (#30922, `7bd180830`), Qwen3-VL accurate
  video prompt template (#31619, new `MTMD_VIDEO_TEMPLATE_QWEN3VL`, helper path only), log-mel computed in the
  compute graph (#31808), one compute buffer shared among sub-contexts (#31945, less mmproj memory), and stricter
  mmproj validation: MLP projector + patch embedding required (#32129), Qwen-VL 2x2 spatial merge required
  (#32130), HunyuanVL F32 position embedding (#32131), MiniCPM-V projection width from `ne[1]` (#32132).

### New Text Model Architectures
| Model | PR | Notes |
|-------|-----|-------|
| Metis-1.6 Praxis (`metis16`) | #31134 `9309bf1bc` | **2.94B** hybrid Mamba-2 + attention MoE, each token runs the stack twice (0.46B active per pass); thinking on/off; 128K context; Apache-2.0. Only iOS-size new arch. |

Larger (one line): Metis-1.6 Logos 10B (same arch), Xing4.0 29B-A4B (`xing4_0`, #31589), Clover 35B-A3B
(`clover_moe`, hybrid attention + recurrent MoE, #31306), Ripple 65B (`ricursive`, SWA + RoPE/NoPE pattern,
#31692), Kimi-K3.1-Pro 2.8T-A50B (`kimi_k31`, DSA indexer, #31779).

### Metal / Apple Silicon
- **Gemma 4 graph optimizations — large prompt-processing gains for E2B / E4B** (#31356, `6cb3e672e`); we ship
  `gemma-4-E4B-Q4_0`.
- Metal: fusable node groups never split across command buffers, so output no longer depends on graph size
  (#31688); `mul_mm` used for 32 ≤ K < 64 (#30982); fused DeepSeek-V4 / GLM MoE routing (#31615).
- Metal safety: out-of-bounds writes to the flash-attention output buffer (#31662) and reads in SOLVE_TRI
  (#32103) fixed; GROUP_NORM with ne03 > 1 / non-contiguous inputs (#31852); cpy into non-contiguous dst (#31581);
  deterministic Top-K ties (#31084).
- **`vm_allocate` instead of `posix_memalign` for host buffers on all Apple platforms** (#31437) — host buffers
  are returned to the OS on free.
- ARM NEON: 4x4 `simd_gemm` tile (#32128). CPU: F16 matmul now takes F32 activations (`ggml_vec_dot_f16_f32`,
  #31854) and CPU flash attention accumulates V in F32 (#29810) — accuracy on the CPU path.
- Unified KV for several sequences: `flash_attn_ext_rows` builds per-sequence row lists (#29510).

### Runtime / KV cache / Jinja
- **KV shifting fixed** with per-model `build_rope` and new tests (#31683, `6023692b1`) — our context shift uses it.
- FP8 (E4M3) KV cache (#28392) — CPU/CUDA; **Metal has no F8_E4M3 kernels** (GET_ROWS / mul_mv refuse it), so
  not usable on Apple yet.
- MoE expert cache: run missed experts on CPU (`moe_cache_cpu_share`, #30966), LRFU cache (#31233) — desktop
  offload feature, not used by us.
- NextN / MTP / DFlash2 speculative decoding APIs (#31927, #30141) — see API section.
- `kv-cache`: never 0 rows for a sequence without cells (#31570); graph result reuse across scheduler reserves
  (#32089), scheduler reuse when params are compatible (#32009), unused graph metadata pages not touched (#32043).
- Jinja: chained comparisons + argument unpacking (#32072), `map('filter')` (#24033); chat: Gemma 4 loose schema
  rules (#31915), TTS text normalizer via jinja (#32053). Our templates render through `ChatTemplate.swift`
  (swift-jinja) and tool calls through `ai/tool/parser/`, so upstream `common/chat` changes do not reach the app.
- gguf-py: every tokenizer merge pair validated (#31631); external chat templates win over tokenizer_config
  (#31717). Quantize: imatrix file path no longer leaks into GGUF metadata (#32000).

### Server / UI (not shipped in the app)
OAI `/v1/audio/speech` (#31763), Responses API json_schema (#26145), realtime audio, new web UI sidebar/search.

---

## API Changes

### `include/llama.h`
- **Removed (#30565 prune)**: `llama_load_model_from_file`, `llama_free_model`, `llama_new_context_with_model`,
  `llama_n_ctx_train/n_embd/n_layer/n_head/n_vocab`, `llama_get_state_size`, `llama_copy_state_data`,
  `llama_set_state_data`, `llama_load_session_file`, `llama_save_session_file`, `llama_set_warmup`, all
  `llama_token_*` vocab accessors (`llama_token_bos/eos/eot/cls/sep/nl/pad/get_text/get_score/get_attr/is_eog/
  is_control`, `llama_token_fim_*`), `llama_add_bos_token/add_eos_token`, `llama_vocab_cls`,
  `llama_sampler_init_grammar_lazy` — **bridge: none used in compiled code** (only in comments, and in
  `Sources/cpp/gpt_spm.cpp`, which `Package.swift` does not compile). **`MacCommandLine/main.swift` used
  three of them → fixed (P7)**.
- **Removed field** `llama_context_params.defrag_thold` — not used (bridge starts from
  `llama_context_default_params()`; no memberwise init).
- **Added field** `llama_context_params.moe_cache_cpu_share` (float) — defaulted; no action.
- **Added** NextN / speculative: `llama_model_dflash_selector_top_k`, `llama_model_target_layer_ids[_n]`,
  `llama_set_embeddings_nextn[_dev]`, `llama_set_nextn_layer_offset`, `llama_get_embeddings_nextn[_ith|_rows]`,
  `llama_set_embeddings_nextn_rows`, `llama_set_embeddings_layer_inp[_dev]`, `llama_get_embeddings_layer_inp`,
  `llama_supports_embd_row`, `llama_set_ctx_state_src`, `llama_get_ctx_other`, `llama_batch_ext_add_embd_row`,
  `llama_batch_ext_set_embd_state_row` — opportunity only.
- `llama_batch.logits` comment only. `LLAMA_SESSION_VERSION` 11 / `LLAMA_STATE_SEQ_VERSION` 4 unchanged.

### `ggml/include/ggml.h`
- **Removed (#29801 prune)**: ops `GGML_OP_ADD1`, `GGML_OP_DIAG_MASK_INF/ZERO` and `ggml_add1[_inplace]`,
  `ggml_diag_mask_*`, `ggml_type_sizef`, `ggml_mul_mat_set_prec`, `ggml_rope_custom[_inplace]`,
  `ggml_rope_yarn_corr_dims`, `ggml_upscale_ext`, `ggml_flash_attn_ext_set_prec` — no consumer use (our PrismML
  kernels do not reference them, P3).
- **Added** `ggml_rope_set_truncate` (YaRN `truncate=false`, #30206), `ggml_flash_attn_ext_rows` (#29510).
- **No new `GGML_TYPE_*` / `GGML_FTYPE_*`**: F8_E4M3 = 43 already existed at b12015; PrismML PQ2_0 = 142,
  PTQ1_0 = 143, COUNT = 144 unchanged → `GGUFType` (`libs/GGUFileStructure.swift`) needs nothing.

### `ggml/include/gguf.h`, `tools/mtmd/clip.h`
`gguf.h` unchanged. **`clip.h` deleted** (#31865) — the bridge never imported it and it was never in the
framework module map; the build script stopped copying it (P3).

### `tools/mtmd/mtmd.h`
- **Added** `MTMD_VIDEO_TEMPLATE_QWEN3VL`; `mtmd_bitmap_is_voice`; gen-audio `MTMD_GEN_AUDIO_TYPE_CHATTERBOX`,
  `MTMD_GEN_AUDIO_TYPE_DRAMA3`; `mtmd_gen_audio_info.n_voices`; `mtmd_gen_audio_read_tensor`,
  `mtmd_gen_audio_voice_name`, `mtmd_gen_audio_get_voice`, `mtmd_gen_state_set_ref`.
- Thread-safety documented: `mtmd_encode_chunk` stores its output in ctx (**not** callable from several threads at
  once); `mtmd_batch_encode`, audio stream flush / next and `mtmd_gen_audio_process` are thread-safe per object.
  The bridge drives one `mtmd_ctx` serially through `mtmd_helper_eval_chunks` — no change.
- `mtmd_context_params` unchanged (bridge sets `n_threads`, `use_gpu`, `print_timings` on the default struct).

### `tools/mtmd/mtmd-helper.h`
- **Added** streaming resampler `mtmd_helper_resampler_init/free/process` (#31964).
- **Changed** `mtmd_helper_gen_audio_inp`: **`voice` removed** (pass a voice bitmap from
  `mtmd_gen_audio_get_voice` as `speaker_ref`), **`temp` added** (#31655). The bridge has no gen-audio code.
- C++ only: `mtmd_helper_video_src/seq` test internals. `mtmd_helper_bitmap_init_from_file(..., opt)` unchanged.

### State Save/Load Behavioral Changes
- Format versions unchanged (session 11, seq 4); `llama_state_save_file` / `llama_state_load_file` (used by
  `LLaMa.swift` / `GPT_SPM.swift`) behave as before. **No session-cache invalidation needed.**
- #30541 (`e56f006ed`): on-device per-sequence state copies (`LLAMA_STATE_SEQ_FLAGS_ON_DEVICE`) used the block
  count instead of the element count for quantized KV (q8_0 / q4_0) — restored state was wrong or crashed. We
  do not use the on-device flag, but `kv_cache_type_k/v` = q8_0 / q4_0 is a user option, so this is a fix in a
  path we could hit.
- #30141: MTP hidden state saved and restored (speculative only).

---

## Risk Assessment — 6 risks (0 high, 3 medium, 3 low)

### MEDIUM: Stricter mmproj validation may refuse a catalogue projector
**Problem:** #32129 / #32130 / #32131 / #32132 reject mmproj files missing the MLP projector / patch embedding,
Qwen-VL projectors without a 2x2 merge, HunyuanVL without an F32 position embedding. An old third-party mmproj in
the catalogue could now fail `mtmd_init_from_file` (the bridge logs and returns false).
**Verify:** P10 `run_model_tests.py --gguf-only --scan-local` covers every VL row — watch `HunyuanOCR-Q4_K_M` (#32131), `MiniCPM-V-4.6-Q8_0` (#32132) and the Qwen-VL-projector rows (#32130); plus `GGUFVisionModelTests`
and `VideoFrameMtmdTests` on the macOS test host. **Status:** P10.

### MEDIUM: Context shift path rewritten (#31683)
**Problem:** KV shifting moved to per-model `build_rope`. `LLaMa.swift` (`llama_memory_can_shift` →
`llama_memory_seq_rm` / `llama_memory_seq_add`, lines ~884-942) relies on it when a chat overflows `n_ctx`.
A fix upstream, but a different code path for every architecture.
**Verify:** a long generation past `n_ctx` on one dense model (e.g. `SmolLM3-Q4_K_M` at `context` 512; not Qwen3.5 / LFM, whose recurrent layers cannot shift) produces
coherent text after the shift; P10 generate sweep. **Status:** P10 / manual.

### MEDIUM: mtmd preprocessing changes (audio log-mel in graph, Gemma 4 no letterbox, shared compute buffer)
**Problem:** #31808 moves log-mel to the compute graph (audio models: LFM3-Audio, Gemma 4 E4B, Qwen-Omni);
#30922 changes Gemma 4 image resizing; #31945 shares compute buffers. Output can shift slightly.
**Verify:** P10 vision + audio rows in the GGUF sweep; `GGUFVisionModelTests` on the macOS host. **Status:** P10.

### LOW: API prune — `MacCommandLine` sample target
Three removed calls replaced (`llama_model_load_from_file`, `llama_model_free`, `llama_init_from_model`). The
target is not in the AIAssistant / AIAssistantMac schemes, so it never blocked the app build.

### LOW: Host buffers via `vm_allocate` (#31437)
Memory accounting on iOS changes shape (pages returned on free). Watch `device_ram.md` budget logs on the next
device run; no action.

### LOW: CPU F16 matmul in F32 (#31854)
CPU-only path (simulator, `n_gpu_layers = 0`) slightly slower, more accurate. No action.

---

## New models for the catalogue (≤ 4B, candidates only — nothing downloaded)

| Model | Size / params | Decision | Reason |
|---|---|---|---|
| **Metis-1.6 Praxis** — `Lernex/Metis-1.6-Praxis-GGUF`, `Metis-1.6-Praxis-Q4_K_M.gguf` (2019 MB; also IQ4_XS 1836 MB, Q8_0 3224 MB), no mmproj | 2.94B (0.46B active per pass), 128K ctx | **Add** | Only new iOS-size text arch (`metis16`). Apache-2.0, not gated. Chat template has tools + thinking toggle → run the `--tool-call` sweep; hybrid Mamba-2 means `llama_memory_can_shift` = false (no context shift), as for our other recurrent rows. Suggested `context` 8192. Category: reasoning / small. |
| **Mage-VL-Cowork** — `ggml-org/Mage-VL-Cowork-GGUF`, `Mage-VL-Cowork-Q4_K_M.gguf` (2716 MB) + `mmproj-Mage-VL-Cowork-Q8_0.gguf` (354 MB) | 4.74B total (Qwen3-4B text, 36 layers, 2560 hidden + 0.7B ViT) | **Decision for user** (recommend Add to test tier if P8 vision test passes) | New `magevl` encoder. Apache-2.0. Text model is exactly 4B (same text size as our Qwen3.5-4B / gemma-4-E4B rows), ~3.1 GB on disk. Tuned for computer-use / GUI agents (OSWorld 66.0), not general chat — value as a general VLM unproven. Base `microsoft/Mage-VL` (`MageVLForConditionalGeneration`) is **not** convertible upstream; only the Cowork variant. |
| **Chatterbox Turbo** — `ggml-org/chatterbox-turbo-GGUF`, `chatterbox-turbo-Q8_0.gguf` (482 MB) + `mmproj-chatterbox-turbo-F16.gguf` (555 MB) | ~0.35B backbone + generator | **Decision for user** | TTS (English) via mtmd gen-audio; MIT. Needs new app code (bridge has no `mtmd_gen_audio_*` / `mtmd_helper_gen_audio` driver) and a `SpeakerType`; voice cloning via `speaker_ref`. Competes with our Core AI Kokoro / MLX audio TTS. |
| **Chatterbox Multilingual V3** — `ggml-org/chatterbox-GGUF`, `chatterbox-Q8_0.gguf` (559 MB) + `mmproj-chatterbox-F16.gguf` (595 MB) | ~0.5B | **Decision for user** | Same as above, 23 languages; MIT. |

Out of size (one line): Nemotron 3.5 Transcribe 30B-A4B (ASR, only size published), Drama 3 35B-A3B (TTS),
Metis-1.6 Logos 10B, Xing4.0 29B-A4B, Clover 35B-A3B, Ripple 65B, Kimi-K3.1-Pro — **Reject: > 4B**.

## PrismML triage

`prismml/prism` `9d9efeadb` → `6447906bf` (2026-11-06): 222 uncarried total, 32 new, **none picked**. 15 CUDA / CI
/ build, 8 merges, 6 DSpark / DFlash / tests, 3 GDN / qwen35 graph (excluded family), `a2008ca9e` fixes batched
uploads we do not carry. Upstream #29772 absorbed our last Vulkan FWHT delta. Still carried: PQ2_0 / PTQ1_0
types and CPU / Metal / CUDA / Vulkan kernels, loader / quant / gguf hooks, tests.
**Candidate `6447906bf` "metal: use more command buffers on iOS" (#427):** defaults Metal `n_cb` to 8 under
`TARGET_OS_IPHONE` (we use 1, `ggml-metal.cpp:703/798`) so a long prompt batch is not killed by the iOS GPU
watchdog (`kIOGPUCommandBufferCallbackErrorInnocentVictim`, `llama_decode` = -3). Needs a device test with a
long prompt before picking; listed under Opportunities.

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| `build-xcframework.sh` | Unchanged in range | — |
| mtmd sources | `libmtmd` from new `MTMD_CORE_SOURCES` (`mtmd`, `-audio`, `-image`, `-helper`, `-helper-gen`, `-model`, `-subcontext`, `-graph`) + 5 new `models/*.cpp` | Copied into `src/` / `src/clip-models/` and compiled into `libllama`. Ported in `dc4c4b127`: `clip.cpp` → `mtmd-model.cpp`, `mtmd-subcontext.cpp`, `mtmd-graph.cpp` in `src/CMakeLists.txt` (+ unity exclusions); copy/clean lists; 5 encoder copy lines; `models.h` include sed now patches 4 files; `clip.h` no longer copied to Headers |
| Versions | `LLAMA_VERSION` 0.8 → 1.0, `GGML_VERSION` 0.28 → 1.0 | No action |
| New CMake options | none relevant to Apple | — |

Copy list equals `tools/mtmd/models/*.cpp` (`comm` check empty, P3).

## Conflicts and fork patches

| File | Resolution |
|---|---|
| `ggml/src/ggml-vulkan/vulkan-shaders/fwht.comp` | Took upstream (#29772 `A_TYPE` form covers our four `fwht[_shmem]_{f32,f16}` pipelines) |
| `ggml/src/ggml-vulkan/vulkan-shaders/vulkan-shaders-gen.cpp` | Took upstream (Vulkan not built for Apple) |
| `src/CMakeLists.txt`, `build-xcframework-ios.sh`, `.gitignore` | Silent: ported to the clip → mtmd split (#31865), tagged `wangqi modified 2026-11-13` |

Fork delta byte-identical outside the two Vulkan files (2409 lines). `wangqi modified` inventory equal for every
file (ggml.h 2, arch-fallback.h 1, quants.h 1, cuda common.cuh 1 / mmq.cuh 1 / mmvq.cu 1, ggml-metal-ops.cpp 4,
constants.py 3, llama.h 1, llama-model-loader.cpp 2, llama-vocab.cpp 1); changed only `src/CMakeLists.txt` 5 → 6
and `build-xcframework-ios.sh` 3 → 8 (today's port). Nothing dropped, absorbed (other than the Vulkan FWHT
shaders) or restored. Metal FWHT: no overlap (upstream owns it since b11222).

## Dependencies
No dependency axis. `src/miniaudio/miniaudio.h` (our tracked copy, it shadows `vendor/`) refreshed
v0.11.22 → 0.11.26 to match upstream's vendored copy (#31459), our commit `6a0797808`. cpp-httplib 0.66.0,
BoringSSL, LibreSSL bumps are server-only, not in the xcframework.

## Opportunities (not built)

| Upstream feature | Consumer that would use it | Value |
|---|---|---|
| PrismML `6447906bf` Metal `n_cb` = 8 on iOS | `ggml-metal.cpp` (fork patch) | Stops the iOS GPU watchdog killing long prompt batches (`llama_decode` -3); needs a device test |
| Chatterbox TTS (`mtmd_gen_audio_*`, `mtmd_helper_gen_audio`, voices, `speaker_ref` cloning) | new `LLaMa_MModal` gen-audio driver + `SpeechHelper` `SpeakerType` | On-device multilingual TTS with voice cloning from the llama engine |
| Nemotron 3.5 Transcribe / Gemma 4 audio stream + `mtmd_helper_resampler` | `ASRHelper` live transcription via `mtmd_audio_stream_*` | Streaming ASR on a model we already ship (Gemma 4 E4B); resampler removes our own resampling |
| `MTMD_VIDEO_TEMPLATE_QWEN3VL` / `mtmd_helper_video` | `LLaMa_MModal.createBitmapsFromVideo` | Qwen3-VL-exact timestamps; today we feed raw frames (MTMD_VIDEO off, no ffmpeg) |
| NextN / MTP / DFlash2 speculative APIs | `LLaMa.swift` decode loop | Faster decode on models with MTP heads (none in our ≤ 4B catalogue yet) |
| FP8 KV cache (#28392) | `ModelParams.kv_cache_type_k/v` | Not usable: Metal lacks F8_E4M3 kernels |
| `moe_cache_size` / `moe_cache_cpu_share` | `llama_context_params` | Desktop expert offload; not relevant on iOS |
| Metis-1.6 Praxis thinking toggle | `ai/thinking/` level → template flag | Small reasoning model with thinking off/on |

## Verification
- P3 quick gate: Ternary-Bonsai-1.7B-PQ2_0 generate PASS.
- P5: xcframework built (iOS, iOS sim, macOS, Mac Catalyst); encoder copy list equals `tools/mtmd/models/`;
  `GGUFTypeParityTests` PASS.
- P10 generate (`--scan-local`, final run on swift-jinja `225f0f0`): **87 passed / 1 failed / 0 skipped of 88**
  (`20261113-142959`) vs baseline 85 / 1 / 0 of 86 (`20261101-091121`, ab9aa10a9). No REGRESSED.
  Standing FAIL `PaddleOCR-VL-1.6-Q4_K_M`: an uncatalogued local file picked up by `--scan-local`; the OCR model
  emits EOS on the text-only probe (failed identically on the baseline). The shipped `PaddleOCR-VL-1.6` (Q8_0) and
  `-1.5` rows PASS.
- P10 tool-call: **31 passed / 0 failed / 12 skipped of 43** (`20261113-143818`) vs baseline 29 / 0 / 13 of 42.
  Fixture findings: LFM2.5-VL-450M now emits `offset=10` (was `-10`, its expectation reason) and PASSes;
  LFM2-8B-A1B now ends its reply in a tool call; SmolLM3 only the prompt date moved.
- Unit suites (iOS sim, slot 9): ToolCallReplayTests, CoreAIToolCallFallbackTests, OCRChatTemplateTests,
  OCREngineCatalogTests, GGUFTypeParityTests, GGUFProjectorRoleTests, GGUFImportCoordinatorTests,
  GGUFRepoCatalogTests, DownloadManagerGGUFClipRecordTests: all PASS.
- macOS host: GGUFVisionModelTests + VideoFrameMtmdTests + Flux2PromptEncoderTests: 24 tests, 0 failures,
  1 skipped (Youtu-VL-4B mmproj absent, standing). Both app schemes build.
- Device-only, not verified: PrismML `6447906bf` (`n_cb` 8 on iOS), `vm_allocate` host-buffer memory shape,
  context shift past `n_ctx` on a device.

## Catalogue outcome (P8)
- **Added** `Metis-1.6-Praxis-Q4_K_M` (Reasoning, badge 3.4.3, all four tiers, test synced; mirror
  `flyingfishinwater/good_and_small_models`). Its template uses `{% for call in message.tool_calls %}`; our
  swift-jinja treated `call` as a keyword. Fixed in swift-jinja `225f0f0` (branch `tag-20261113`): `call`,
  `filter`, `macro`, `generation` accepted as names, as in Jinja2. Older app binaries cannot render this model's
  template, so the row reaches prod only with this binary.
- **Rejected after test** `Mage-VL-Cowork-Q4_K_M` (user approved the candidate): generate + vision PASS, tool-call
  FAIL with schema-echo arguments (`{"arguments": {"properties": {...}}}`) and a looping `<think>` that replays
  trajectory-training text. The user removed the GGUF + mmproj from the mirror and the local volume on
  2026-11-13 (mirror commit 015572b).
- Chatterbox Turbo / Multilingual: decision for the user (needs an mtmd audio-generation driver + a speaker).

## Action Items

1. **REQUIRED**: commit `MacCommandLine/main.swift` (app repo) with the upgrade; no `llamacpp_swift` change needed.
2. **REQUIRED after building**: none for session caches (format unchanged).
3. **Recommended**: P10 full GGUF sweep with VL + audio rows; a long-context run past `n_ctx` to exercise the
   rewritten context shift; device test of PrismML `6447906bf` before a later pick.
