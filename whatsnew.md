# llama.cpp Upgrade: b9165 → b9279

**Date:** 2026-05-22
**Commits in range:** 120 upstream commits merged

---

## New Features

### Vision Model Changes
- **HunyuanOCR merged into HunyuanVL** (#23329): `tools/mtmd/models/hunyuanocr.cpp` removed, replaced by `hunyuanvl.cpp`. Also fixes OCR vision precision regression.
- **DeepSeek-OCR image processing fixes** (#23345): img_tool::resize padding refactored; better aspect handling.
- **mtmd::qwen3a chunks + preprocessing fix** (#23073): correct preproc for Qwen3 audio/omni multimodal chunks.
- **mtmd::fit_params now considers mmproj** (#21489): mmproj weight memory factored into automatic context sizing.

### New Text Model / Tokenizer Support
| Item | PR | Notes |
|------|-----|-------|
| Carbon-3B HybridDNATokenizer | #23410 | New vocab type for DNA-domain LLM |
| HybridDNA tokenizer fix | #23466 | Correctness fix for the above |
| NvFP4 quantized LM head | #23046 | NVIDIA FP4 head support |

### Metal / Apple Silicon
- **`metal: optimize concat kernel and fix set kernel threads`** (#23411) — faster concat ops, correct dispatch.
- **`metal: optimize pad + cpy`** (#23354) — faster image-padding paths used by vision encoders.
- **`metal: tighten input-position loop in kernel_conv_transpose_1d`** (ggml/1477) — smaller register pressure in audio/conv_transpose.

### MTP (Multi-Token Prediction / Speculative Decoding)
A substantial MTP feature landed across this range:
- `llama + spec: MTP Support` (#22673) — full MTP plumbing.
- `Move to backend sampling for MTP draft path` (#23287).
- `mtp: use inp_out_ids for skipping logit computation` (#23433).
- `llama: avoid copying logits during prompt decode in MTP` (#23198).
- `MTP clean-up` (#23269), `clarify MTP layer comment in qwen35.cpp` (#23338).

### Server / UI / Misc
- Server: free draft/MTP resources on sleep to fix VRAM leak (#23461).
- Server: expose prompt token counts in `/slots` (#23454).
- Vendored `cpp-httplib` bumped to 0.45.0 (#23103).
- Major UI restructure to `tools/ui/` (#23064) — irrelevant for iOS framework.
- ggml bumped to version 0.12.0.
- WAV MIME type improvements + audio format detection (#23396).

### Stability / Correctness
- `llama-graph: fix null-buffer crash in llm_graph_input_attn_kv_iswa for SWA-only models` (#23131).
- `llama: initialize pre-norm embedding mask flag` (#23256).
- `ggml: Check the right iface method before using the fallback 2d get` (#23306).
- `common/speculative: fix nullptr crash in get_devices_str` (#23386).
- `server-context: guarantee there is at least 1 token to decode` (#23280).
- `fix(flash-attn): replace f32 with kv_type and q_type` (#23372) — Flash attention now respects KV/Q dtypes instead of hardcoded f32.

---

## API Changes

### `include/llama.h`
- **Added** enum `llama_context_type` with values `LLAMA_CONTEXT_TYPE_DEFAULT` (0) and `LLAMA_CONTEXT_TYPE_MTP` (1).
- **Added** `llama_context_params.ctx_type` (`enum llama_context_type`) — selects MTP vs default context.
- **Added** `llama_context_params.n_rs_seq` (`uint32_t`) — recurrent-state snapshots per seq for rollback (0 = no rollback). **EXPERIMENTAL**.
- **Added** `llama_n_rs_seq(const llama_context *)`.

Both new struct fields keep the existing fields in place, but **struct layout has changed**. Any Swift bridge that zero-initializes `llama_context_params` via `llama_context_default_params()` is safe; any code that builds the struct field-by-field must add the new fields. Our `llamacpp_swift` bridge uses `llama_context_default_params()` and is unaffected.

### `tools/mtmd/clip.h`
- **Added** `clip_context_params.no_alloc` (bool) — skip backend allocation during init.
- **Added** `clip_get_mem_usage(const clip_ctx *) → std::map<ggml_backend_dev_t, size_t>` (replaces `clip_has_whisper_encoder`).
- **Removed** `clip_is_minicpmv`, `clip_is_glm`, `clip_has_whisper_encoder` — all unused by our bridge.

### `tools/mtmd/mtmd.h`
- **Added** `mtmd_get_memory_usage(mmproj_fname, ctx_params) → std::map<ggml_backend_dev_t, size_t>` (C++ only). Marked unstable / will change without deprecation.

### State Save/Load Behavioral Changes
- `save-load-state` test refactor (#23196, #23336) reorganizes the harness but does not alter the on-disk format. **No session cache invalidation required.**

---

## Risk Assessment

### LOW: HunyuanOCR file rename
File `hunyuanocr.cpp` removed in favor of `hunyuanvl.cpp`. Build script patched. No app-side change needed; the unified `mtmd_helper_eval_chunks` flow already picks up the new graph.

### LOW: New `llama_context_params` fields
`ctx_type` and `n_rs_seq` added. Our bridge initializes params via `llama_context_default_params()` so defaults (`LLAMA_CONTEXT_TYPE_DEFAULT` = 0, `n_rs_seq` = 0) are picked up automatically.

### LOW: clip.h removals
`clip_is_minicpmv`, `clip_is_glm`, `clip_has_whisper_encoder` removed. We do not call any of these from `llamacpp_swift` or app code.

### LOW: MTP plumbing
MTP is opt-in via `ctx_type = LLAMA_CONTEXT_TYPE_MTP`. We default to non-MTP; no behavior change.

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| mtmd model copy | via CMake glob | explicit `cp -fp` list (renamed file patched today) |

**Structural changes:** one-line rename in `copy_mtmd_files()` — `hunyuanocr.cpp` → `hunyuanvl.cpp`.

---

## Action Items

1. **REQUIRED before building**: `build-xcframework-ios.sh` already patched.
2. **Recommended**: rebuild the xcframework (`thirdparty/llama.cpp/build-xcframework-ios.sh`) and smoke-test a vision model (HunyuanVL OCR path in particular).
3. **No session cache invalidation** required.

---

## Local Patches (alongside FA nsg patch)

### Retained Swift-side diagnostics — 2026-05-22
- `thirdparty/llamacpp_swift/Sources/swift/LLMBase.swift`: `[PERF]` summary line plus `llama_perf_context` / `llama_perf_sampler` dump on session end. Cheap, aids future debugging. C-level log forwarding to `LlamaLogCollector` was already installed by `LLaMa.swift`'s 2026-04-08 static initializer; no separate installer added.

### Restore cpy threadgroup occupancy regressed by upstream `metal: optimize pad + cpy` (#23354) — 2026-05-22
- `ggml/src/ggml-metal/ggml-metal-ops.cpp` in `ggml_metal_op_cpy`: b9279 changed `nth` to `std::min<int>(nk0*ne01, 256)` and hard-coded the `nrptg*nth > 256` bound. On Apple A18 Pro the cpy pipeline reports `max_tg=1024`, so the 256 ceiling halved threadgroup occupancy whenever `nk0 >= 512` (visible as `[DISPATCH cpy] nk0=512 nth=256` in `app1.txt`). Restored the b9165 formula `nth = std::min<int>(nk0, ggml_metal_pipeline_max_theads_per_threadgroup(pipeline))` and matched the inner bound to the pipeline cap. Confirms ~12–17 % gen-throughput recovery on Bonsai-8B-Q1 (see Phase 3 comparison in `helper/docs/plans/diagnose-llamacpp-b9279-prediction-slowdown.md`). Kept the EOF guard and the new `kargs_cpy` packing introduced in b9279 — they are correctness fixes orthogonal to the occupancy regression.
