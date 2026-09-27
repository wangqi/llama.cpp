# PrismML picks in this fork — what is carried, and what a weekly upgrade must do

_This file is ours, not upstream's. It lives in the submodule so it is in front of you while you are
working in this repo during an upgrade._

**Audience:** whoever runs the weekly `llama.cpp` upstream merge.

**One-line summary:** this fork carries **23 cherry-picked commits** from
`github.com/PrismML-Eng/llama.cpp` branch `prism`, on branch `prism-pq2`, adding the `PQ2_0` /
`PTQ1_0` quant types and the `prism.hadamard` activation-rotation runtime. **They are ordinary
commits in our history. Do not re-pick, re-merge or "restore" them on an upgrade.**

The design rationale, the full commit table and the exclusion list live in
`helper/docs/llama_cpp_prism.md` in the app repo. This file is the operational half.

---

## 1. The rule that matters most

We **merge** upstream into this fork; we do not rebase onto it. So the PrismML picks behave exactly
like the `// wangqi modified` patches: they are already in history, and an upstream merge either
leaves them alone or raises a conflict.

| Situation | What to do |
|---|---|
| Upstream merge touches none of the files below | Nothing. The picks are fine. |
| Upstream merge conflicts in one of the files below | Resolve by hand using §4. **Never** resolve by taking the PrismML side wholesale. |
| A pick appears "missing" after a merge | It is almost certainly not. Run §2 before concluding anything. |
| PrismML published new work | Triage it with §3. Do not bulk-pick. |

There is **no re-application step**. If you find yourself running `git cherry-pick` against
`prismml/prism` during a routine upgrade, stop and re-read this file.

---

## 2. Verify the set is intact (30 seconds)

```bash
cd thirdparty/llama.cpp
BASE=04e243217   # the v0.4.1 pin this set sits on. Use the SHA: a *branch* named v0.4.1
                 # also exists, so the bare tag name is ambiguous and git will warn.

git log --grep="cherry picked from commit" --oneline $BASE..HEAD | wc -l   # expect 23
```

The branch carries **24** commits over that base: the 23 picks plus one that annotates the
hand-resolved conflicts.

Every pick was applied with `git cherry-pick -x`, so each message ends with
`(cherry picked from commit <sha>)`. That trailer is the provenance record and the reason the set
is self-describing — keep using `-x` if you ever add to it.

A functional check that does not depend on git history at all:

```bash
grep -n "GGML_TYPE_PQ2_0\|GGML_TYPE_PTQ1_0\|GGML_TYPE_COUNT" ggml/include/ggml.h
# GGML_TYPE_PQ2_0  = 142
# GGML_TYPE_PTQ1_0 = 143
# GGML_TYPE_COUNT  = 144
```

If `GGML_TYPE_COUNT` is back to 43, the picks really are gone — recover them from the reflog or
from `origin/prism-pq2`, do not re-pick from `prismml/prism` (you would lose the conflict
resolutions).

---

## 3. See what PrismML has published since

```bash
cd thirdparty/llama.cpp
BASE=04e243217
git fetch prismml prism

# The 23 SHAs we carry. Take the LAST cherry-pick trailer in each message: PrismML's own
# commit bodies contain nested trailers, so a naive grep over the range returns 41, not 23.
git log --grep="cherry picked from commit" --format=%H $BASE..HEAD | while read c; do
    git log -1 --format=%B "$c" | grep -o "cherry picked from commit [0-9a-f]\{40\}" | tail -1 | awk '{print $NF}'
done | sort > /tmp/ours
wc -l < /tmp/ours                      # 23

git log --format=%H prismml/prism --not upstream/master | sort > /tmp/theirs
comm -13 /tmp/ours /tmp/theirs          # in prism, not carried by us
```

As of 2026-09-18 that last command prints **67** commits; on 2026-09-27 it printed **104**, triaged in
`whatsnew.md` (only `0324c6652` is a candidate). **That is the expected steady state, not
a backlog.** Almost all of it is deliberately excluded — see the exclusion table in
`helper/docs/llama_cpp_prism.md`. The short version:

- **GDN ring decode + graph fusions** — rewrites `src/models/qwen35.cpp` and the
  `ssm_conv`/`ssm_scan` Metal pipeline signatures, the path every hybrid model in our catalogue
  runs on *today*, for a claimed +2.6% decode. Not worth it.
- **DSpark / DFlash / DFly drafters (~25)** — speculative decoding is not enabled in our bridge.
- **CUDA / HIP / Vulkan / SYCL (~30)** — not built by `build-xcframework-ios.sh`.
- **CI workflows, README, AGENTS.md** — theirs, not ours.

Only pick something new if it fixes correctness on `PQ2_0`, `PTQ1_0` or the Hadamard path.

**PrismML rebases.** After a rebase their SHAs go unreachable and `merge-base` collapses. They keep
`prism-b<n>-<sha>` release tags; the `-x` trailers preserve the original SHAs regardless. If
`comm` suddenly reports ~1,100 commits, you are looking at a post-rebase branch, not new work.

---

## 4. The four files that conflict, and how to resolve them

Only these are files we ship. Anything else that conflicts is CUDA, Vulkan or tests.

| File | Why it conflicts | Correct resolution |
|---|---|---|
| `ggml/src/ggml-cpu/arch-fallback.h` | We add `pq2_0` / `ptq1_0` `_generic` aliases into lists upstream edits constantly | Keep both sides. Check for duplicate `#define`s afterwards. |
| `ggml/src/ggml-cpu/quants.{c,h}` | Our `ggml_vec_dot_pq2_0_*` / `ptq1_0` declarations sit among upstream's | Keep both sides. |
| `src/llama-context.h` | Our `hadamard_verified` flag sits next to upstream's `gf_res_prev_active` | Keep both fields. |
| `ggml/src/ggml-metal/ggml-metal-device.m` | **See the warning below.** | Read it before touching. |

### The `ggml-metal-device.m` trap

This is the one resolution that can fail silently.

PrismML wrote their FWHT-width guard when `GGML_OP_MUL_MAT` and `GGML_OP_MUL_MAT_ID` shared a single
`case` body, so the guard distinguished them internally with `op->op == GGML_OP_MUL_MAT`. **Upstream
has since split those into separate cases.**

Taking the PrismML side verbatim puts the guard inside the `MUL_MAT_ID` case, where
`op->op == GGML_OP_MUL_MAT` is never true — dead code — and simultaneously replaces upstream's
`ggml_metal_supports_mul_mat_op(...)` dispatch with a stale fallback.

The consequence is the dangerous kind: Hadamard-folded F16 matmuls at widths Metal has no FWHT
kernel for would be **accepted and computed without the rotation**. That produces fluent,
on-topic-looking nonsense, not an error, and no test that only checks exit codes will catch it.

**The guard belongs on the `MUL_MAT` case; `MUL_MAT_ID` keeps upstream's dispatch.** The block is
marked `// wangqi modified 2026-09-18` and says so in-source. That also matches PrismML's intent —
their guard only ever applied to `MUL_MAT`.

**Retired in the b11222 merge (2026-09-27).** Upstream adopted the F16-input and wide-width FWHT
work itself (#29094, #29095) and now asks one predicate, `ggml_metal_op_mul_mat_use_fwht()` in
`ggml-metal-common.cpp`, from both `supports_op` and the dispatch. Our guard was dropped in favour
of it. That is safe because the fold's rotation is a materialised F32 Hadamard matrix
(`llama-model.cpp`, `prism.hadamard.<n>`), so any width the FWHT kernel declines still multiplies by
the real rotation on the generic path or the CPU. If this hunk conflicts again, take upstream.

### The b11222 merge (2026-09-27): upstream now owns the FWHT kernels

Twelve files conflicted, almost all because upstream landed its own version of FWHT code we carry.
Resolutions, with the one that git did **not** flag first:

| File | Resolution |
|---|---|
| `ggml/src/ggml-metal/ggml-metal-impl.h` | **Auto-merged with `GGML_METAL_FWHT_TG_MIN_N` defined twice** (ours 512 from PrismML #161, upstream 1024). A redefinition is only a warning; the last one wins. Keeping our `misc.metal` instantiations with upstream's threshold would dispatch the 256-thread threadgroup kernel as a 64-thread simdgroup kernel: wrong output, no error. We took upstream (1024) and deleted ours. After any merge, check each `#define` in this file appears once. |
| `ggml-metal-device.{m,h,cpp}`, `kernels/misc.metal`, the FWHT hunks of `ggml-metal-ops.cpp` | Take upstream. `misc.metal`'s hunk also carries upstream's `FC_dsv4_hc_n_hc` constant, which must survive. Cost: PrismML #161's +3.6% decode at block width 512 (1024 and above use the threadgroup kernel either way). |
| `ggml-metal-ops.cpp`, flash-attention hunk | Upstream replaced `FATTN_SMEM` with an `fa_smem` lambda and still does not cap `nsg`. Our 2026-05-14 iPhone crash fix was re-applied on the lambda. |
| `ggml-cpu/ggml-cpu.c` | Take upstream; same F16-src1 assertion, different wording. |
| `src/llama-context.cpp`, `src/llama-graph.cpp` | Keep both. `graph_params` is positional aggregate init: `hadamard_rotations`, `hadamard_inverses`, then upstream's `prec_policy`, matching `llm_graph_params` in `llama-graph.h`. |
| `conversion/base.py` | Keep both (`add_hadamard_metadata()` and upstream's `prec_a4` block). |
| `ggml-cuda/fwht.cu`, `ggml-cuda.cu` | Not built. Keep our `fwht.cu` (the sign-fused dispatch `ggml-cuda.cu` calls) and add upstream's `ggml_cuda_op_mul_mat_use_fwht()`; take upstream in `ggml-cuda.cu`. |
| `tests/test-backend-ops.cpp` | Keep both, close `test_fwht_signed`'s body, de-duplicate the FWHT case list. |

Verification that caught nothing wrong and is worth repeating: build `test-backend-ops` on the Mac
and run `-o MUL_MAT_HADAMARD -b MTL0` (37/37), `-o MUL_MAT|MUL_MAT_ID|GET_ROWS|CPY -p 'pq2_0|ptq1_0'`
(266/266) and `-o FLASH_ATTN_EXT -b MTL0` (4954/4954).

### Vulkan is declined on purpose

`ggml-vulkan.cpp` and the Vulkan shaders conflicted heavily. We build no Vulkan backend for any
target, so those hunks were dropped wholesale (`git checkout --ours`). Keeping upstream's Vulkan
verbatim also reduces future merge cost, which is the entire point of carrying a curated set. If a
Vulkan conflict appears, resolve it the same way: take upstream.

---

## 5. Two commits are deliberately NOT carried — do not "fix" this

`4ad963fa4` (aarch64 repack fallback aliases) and `cd3f8ed4f` (ARM NEON+DP repack GEMV/GEMM for
Q1_0) were picked during development and then **dropped**, because `cd3f8ed4f` does not compile
here:

```
ggml/src/ggml-cpu/arch/arm/repack.cpp: error: unknown type name 'block_q1_0x4'
ggml/src/ggml-cpu/arch/arm/repack.cpp: error: use of undeclared identifier 'ggml_gemv_q1_0_4x4_q8_0_generic'
```

Those symbols come from PrismML repack infrastructure in a commit outside this set. `4ad963fa4` is
inert without it — nothing here defines or calls `ggml_gemv_pq2_0_4x8_q8_0`, so its aliases are
macros for symbols that do not exist.

Both are ARM repack **optimisations**, not correctness, and `PQ2_0` does not depend on them. If you
ever want them, the missing repack commit has to come too — and then `Bonsai-8B-Q1` (a shipping
catalogue row) must be re-verified, because that is the quant path they touch.

---

## 6. Gates after any upgrade that touches this set

Run in this order. The last one is the blocking gate.

```bash
# 1. The app links the xcframework, not the regression binaries - build it.
cd thirdparty/llama.cpp && ./build-xcframework-ios.sh
```

No build-script or CMake change should ever be needed for this set: it adds no new file under
`ggml/src/ggml-metal/` or `src/`, and all its code lands in `libggml-base` / `libggml-cpu` /
`libggml-metal`, three of the six archives `combine_static_libraries()` merges. If you find
yourself editing the Metal kernel list in `ggml/src/ggml-metal/CMakeLists.txt` because of these
picks, something is wrong.

```bash
# 2. The GGUFType mirror in the app must know every live ggml type (now automated).
cd testcases && ./run_tests.sh --no-build GGUFTypeParityTests          # 4/4

# 3. THE GATE - every model that worked before must still work.
cd helper/scripts/model_regression
./run_model_tests.py --gguf-only --scan-local --download-missing \
    --compare /Volumes/ssd2t/modeltests/baseline-b11222.json
./run_model_tests.py --gguf-only --tool-call --no-build
```

**Pass condition: zero `REGRESSED`, and zero tool-call fixture drift.** Not "no crashes".

`--scan-local` globs the **top level** of `/Volumes/ssd2t/models/`, which is where
`Ternary-Bonsai-2-27B-PQ2_0.gguf` sits — so the Bonsai file is covered with no catalogue edit. The
sweep sorts smallest-first, so the 27B runs last and cannot poison earlier results.

### Reference results (2026-09-18, `b6f8660a7`)

- Pass 1: **75 models, 73 PASS / 2 FAIL, zero REGRESSED.** The two FAILs (`HunyuanOCR-Q4_K_M`,
  `Bonsai-27B-Q1_0`) were **already failing in the pre-change baseline** — they are not regressions,
  and a future run should still show them.
- `Ternary-Bonsai-2-27B-PQ2_0`: PASS, 7 GiB, 12.2 s.
- `Bonsai-8B-Q1`: PASS (the shipping Q1_0 row).
- Superseded as the diff target by `/Volumes/ssd2t/modeltests/baseline-b11222.json` (2026-09-27, 78
  models, 74 PASS / 4 FAIL). Its `meta.llama_cpp_sha` reads `a1a30c5` because the run was made on
  the uncommitted merge tree, which became `12f9cb99a`. Its HunyuanOCR FAIL is stale: the files were
  replaced with HunyuanOCR-1.5 the same day and now pass.

---

## 7. Judging a Hadamard failure by eye — don't

If the rotation is applied wrongly, the model still loads and still emits fluent, on-topic text. It
does **not** error. Two of the carried Metal commits were marked `[UNVERIFIED - needs Apple HW]` by
PrismML themselves; we are the first Metal validation of them.

The real check is not the output, it is the built-in graph verifier. `llama_verify_hadamard_graph()`
in `src/llama-context.cpp` walks the first built graph and **throws** unless every folded weight is
consumed with its activation transform, and every latent table with its inverse:

```
Hadamard-folded weight '<name>' is consumed without its activation transform;
this graph's matmul path does not support prism.hadamard folding
```

A clean load of a `prism.hadamard` model therefore already proves the rotation is wired. If you ever
see that exception after an upgrade, the graph changed underneath the fold — look at
`ggml-metal-device.m` (§4) first.

Measured on an M-series host at 8K context with `q8_0` KV: **7.85 GB peak RSS**, 47.8 tok/s prefill,
19.5 tok/s decode. Note the model's **default `n_ctx` is 262144** — anything that does not set a
context explicitly will try to allocate a ~24 GB shape.

---

## 8. Known gaps (as of 2026-09-23)

<!-- PTQ1_0 has now been run end to end, so the "never run" gap is closed // wangqi modified 2026-09-23 -->
- **`PTQ1_0` runs (closed 2026-09-23).** Loaded with `llama-server -c 8192 -ctk q8_0 -ctv q8_0
  -ngl 99` on an M4 Pro: correct greedy output, 53.9 tok/s prefill, 17.9 tok/s decode, 6.79 GB RSS
  after one request. Also run on device by the developer. Full record in
  `helper/docs/llama_cpp_prism.md`. (Download note kept for the next fetch: HuggingFace Xet
  misbehaved badly on that file — 12 GB transferred, zero completed transmissions;
  `HF_HUB_DISABLE_XET=1` or plain `curl -C -` are the fallbacks.)
- **No catalogue row and no RAM gate.** `Ternary-Bonsai-2-27B` is not in `models_*.json`, and the
  llama path has no memory preflight at all. A 7.2 GB model needs one before it can ship to users;
  reuse `SystemMemoryHelper.canLoadModel(fileSizeMB:)` and copy the MLX policy at
  `ai/AIChatModelMLX.swift`, rather than building a new mechanism.
- **Vision untested.** The `mmproj-Q8_0` (629 MB) uses `qwen3vl_merger`, already supported at
  `tools/mtmd/clip.cpp:975`; it needs no PrismML work, but nobody has tried it.
