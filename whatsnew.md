# llama.cpp Upgrade: v0.4.1 (b11035) → v0.5.0 (b11222)

**Date:** 2026-09-27
**Commits in range:** 186 upstream commits merged (`911f6cdc8` → `a97cce86a`, b11222; `v0.5.0` is
tag `7fe450e19`, so the merge target is past it)
**Merge commit:** `12f9cb99a` on `master`

The previous file in this slot described b10724 → b11035; it is in git history at `04e243217`.

---

## New Features

### New Vision Models
- `ling3vl.cpp` — Ling 3.0 VL (PR #29151). Added to `build-xcframework-ios.sh`; `clip.cpp`
  constructs `clip_graph_ling3vl`, confirmed present in the built xcframework (4 symbols).

Every `.cpp` in `tools/mtmd/models/` is referenced by the build script after this change.

### New Text Model Architectures
No new `src/models/*.cpp` in this range. Converter-only additions (no runtime change):

| Model | PR | Notes |
|-------|-----|-------|
| MiMo-V2.6 | #29257 | `convert_hf_to_gguf.py` support |
| PLaMo-3 YaRN | #29528 | Converter exports YaRN scaling parameters |
| Gemma4 DSpark draft backbone | #29226 | Speculative-decoding draft; not enabled in the app |
| HunyuanOCR DFlash | #28890 | Speculative-decoding draft; not enabled in the app |

### Hadamard / FWHT (the reason this merge conflicted)
Upstream landed its own F16-input and wide-width FWHT kernels on CPU (#27779), Metal (#29094,
#29095), CUDA (#29096) and SYCL (#29243), overlapping the carried PrismML set. See
`prismml_merge.md` §4 for every resolution.

### Metal / Apple Silicon
- Flash-attention vector kernels tuned per chip **family** instead of per SKU (#29075)
- MoE and `SSM_CONV` fusion optimizations (#28948) — hybrid models such as Qwen3.5
- Sparse flash attention optimized (#29377); FA kernels split into per-dtype libraries (#29329)
- Fixes: FA mask bounds in the block pre-pass (#29220), FA support checks (#29122), graph capture and
  empty graphs (#29390), missing f32 × bf16 `mul_mv` variants (#28741), macOS 27 SDK deprecations
  (#29136)
- New ops for Qwen4-exp and DeepSeek-V4 hyper-connections (#29000, #29169)

### CPU
- Tiled `mul_mat` for k-quants (#27851): 3–6× on large matmuls per upstream, but **~80% (a net loss)
  on GEMV** — do not describe this as a decode speed-up.
- ARM repack kernels for `Q1_0` (#23492) — the upstream counterpart of the PrismML repack work
  `prismml_merge.md` §5 declined; `Bonsai-8B-Q1` output unchanged in the regression run.

### Stability
- K/V and recurrent-state cleanup after a failed sequence restore (#27530). The app restores session
  state through `llama_state_load_file` (`LLaMa.swift`), so a failed restore no longer leaves stale
  cache contents behind.
- Allocation failures are checked to prevent crashes (#28149).
- `llama-grammar` numeric truncation on token-id parsing fixed (#29382).

### Jinja / chat (upstream `common/` only)
`dict` builtin, `sameas` test, unary +/-, a Ling 3.0 parser (#28682) and Gemma 4 / Muse Glimmer
tool-grammar fixes. The app renders templates and parses tool calls itself, so none of these reach
it.

---

## API Changes

### `include/llama.h` (additive only, +91 lines)
- **Added**: `llama_batch_ext` API (#24669) — `llama_batch_ext_init/free/clear/add/add_token/
  add_embd/add_seq/set_embd_token/set_embd_state/set_output_*/set_pos`, `struct llama_embd`,
  `enum llama_process_type`, `llama_process()`. The existing `llama_batch` API is unchanged; the
  Swift bridge needs no change.
- **Added**: `LLAMA_VOCAB_TYPE_TEST = 7` (dummy tokenizer for tests).

### `ggml/include/ggml.h`, `gguf.h`, `mtmd.h`, `clip.h`, `mtmd-helper.h`
- No changes in this range. No new `ggml_type`, so `GGUFTypeParityTests` passes unchanged.

### State Save/Load Behavioral Changes
- No session/state format version change. Existing session cache files remain valid.
- A failed restore now zeroes the affected K/V and recurrent state (#27530).

---

## Risk Assessment

### HIGH (resolved): duplicate `GGML_METAL_FWHT_TG_MIN_N`
**Problem:** `ggml-metal-impl.h` auto-merged with the constant defined as 512 (ours) and 1024
(upstream). A redefinition only warns; a mismatch with the `misc.metal` instantiations launches the
wrong kernel shape and yields wrong output with no error.
**Fix applied:** kept upstream's 1024 and upstream's instantiations. `test-backend-ops -o
MUL_MAT_HADAMARD -b MTL0` 37/37.

### HIGH (resolved): flash-attention `nsg` cap would have been lost
**Problem:** upstream replaced the `FATTN_SMEM` macro our 2026-05-14 iPhone crash fix used, and still
does not cap `nsg` nor check threadgroup memory in `supports_op`.
**Fix applied:** re-applied the cap on upstream's `fa_smem` lambda. `FLASH_ATTN_EXT` 4954/4954 on Metal.

### MEDIUM: Metal Hadamard guard replaced by upstream's predicate
**Problem:** the fork's 2026-09-18 guard is gone; upstream's `ggml_metal_op_mul_mat_use_fwht()` now
decides.
**Mitigation:** the fold's rotation is a materialised F32 matrix, so any declined width still
rotates correctly on the generic path or the CPU. `Ternary-Bonsai-2-27B-PQ2_0` / `-PTQ1_0` pass.

### LOW: PrismML #161 width-512 threadgroup kernel dropped
About 3.6% decode at Hadamard block width 512 only. No action required.

### LOW: CPU tiled k-quant path
Net loss on GEMV per upstream's own numbers; watch background (CPU) decode speed on device.

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| mtmd | separate `mtmd` target | copied into `libllama` (`src/clip-models/`) |
| Metal FA kernels | per-dtype libraries (#29329) | same, via CMake — no script change |

**One change:** `ling3vl.cpp` copy line added. `ggml-cpu/tiled/*` and the per-dtype FA `.metal`
files are picked up by upstream CMake with no script change.

---

## Verification

- xcframework: built for iOS device, simulator, macOS, Mac Catalyst.
- Regression pass 1 (`--scan-local --download-missing`, baseline `baseline-prism-pq2.json`):
  **78 models, 74 PASS / 4 FAIL, 0 REGRESSED**; all 73 shared models byte-identical greedy output.
  FAILs: HunyuanOCR (stale pre-b9263 files, since replaced with HunyuanOCR-1.5 and now PASS),
  Bonsai-27B-Q1_0 (already failing), and two Laya encoder files the completion harness cannot run.
- Regression pass 2 (`--tool-call`): **31 models, 19 PASS / 0 FAIL / 12 SKIP** (documented exemptions);
  all 31 `raw_output` fixtures identical.
- `test-backend-ops` on Metal: `MUL_MAT_HADAMARD` 37/37, PQ2_0/PTQ1_0 ops 266/266, `FLASH_ATTN_EXT`
  4954/4954.
- Swift: `GGUFTypeParityTests`, `ToolCallReplayTests`, `GGUFVisionModelTests` (3 pass, 1 skip),
  `VideoFrameMtmdTests` 3/3 and six local-engine suites pass.

---

## PrismML Triage (Step 7.2, 2026-09-27)

`comm -13 ours theirs` now prints **104** (67 on 2026-09-18); all 23 carried picks are still in
`prismml/prism`, so no rebase happened. Of the 37 new commits, 14 touch code we build:

| Commit | What | Decision |
|---|---|---|
| `0324c6652` (#245) | Keep Hadamard rotation tensors out of `CPU_REPACK` buffers (SIGSEGV at load) | **Candidate.** 6 lines in `llama-model.cpp`, applies cleanly. Not reachable with the shipped PQ2_0/PTQ1_0 files (no ARM repack for those types), but reachable for a Hadamard-folded Q4_0/Q8_0/Q1_0 band run CPU-only (background) |
| `078192590` (#257) | Hadamard contract v2: tied output weights | Defer until a v2 file ships; today's runtime rejects v2 loudly (`unsupported prism.hadamard.version: 2`) |
| `df7c49e84` (#262) | Metal PTQ1_0 mat-vec for 2 to 4 columns | Performance only (small batches); not taken |
| `164c33700`, `10df29881`, `d2c9ddd05`, `adfffbe41` | x86 AVX2/VNNI/SSE Q8_K activation path and its follow-up fixes | Not applicable: x86 only, and the fixes repair a path we do not carry |
| `65ac430ab`, `76e7487ce` | Metal 4 tensor-API language version | Upstream Metal area, not PQ2_0/PTQ1_0 correctness |
| `288859a96`, `279df6644` | DFlash / DFlash2 speculative decoding | Speculative decoding is not enabled in the app |
| `ee8ad0ef6`, `49cc6774d`, `ea50aba8c` | Warnings, a one-line style fix, a PR-text commit | Not taken |

---

## Action Items

1. **Done**: conflicts resolved, `ling3vl.cpp` added, xcframework rebuilt, both regression passes.
2. **Recommended**: on an A18 device, run Gemma (head_dim 512) with a quantized KV cache to confirm
   the re-applied FA cap, and compare background decode speed on a k-quant model.
