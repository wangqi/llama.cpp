# llama.cpp Upgrade: b8145 → b8185

**Date:** 2026-03-02
**Commits in range:** 43 upstream commits merged

---

## New Features

### New Vision Models
- No new vision encoder `.cpp` files added. All 18 encoders already present in `build-xcframework-ios.sh`.

### New Text Model Architectures
| Model | PR | Notes |
|-------|-----|-------|
| Jina Embeddings v5 Nano | #19826 | Partial EuroBERT architecture; embedding/retrieval model |

### Performance & Backend
- `ggml-cpu`: add repack for mxfp4 (#19738) — new quantization repack format
- `gguf`: avoid too many `fstat()` file size calls during model loading (#19919) — faster startup
- `server`: enable multi-modal prompt caching (#19877) — vision KV cache reuse
- `server`: support multi-modal context checkpoints (#19849) — resume vision context

### Model & Architecture Changes
- `models`: fix graph splits (#19866) — correctness fix for multi-layer model graph partitioning
- `llama`: add option to merge gate and exp weights (#19139) — new MoE weight merging option
- `vendors`: update miniaudio to 0.11.24 (#19914)

---

## Bug Fixes

### KV Cache & M-RoPE
- `kv-cache`: fix `can_shift()` check to take into account M-RoPE (#19928)
  - Affects models using M-RoPE: Qwen3VL, Llama4, Qwen2VL
  - Without this fix, KV cache shifting could be incorrectly enabled for M-RoPE models

### Multimodal (mtmd)
- `mtmd`: fix padding of n_tokens (#19930)
  - Incorrect token count padding in vision token sequences; could cause subtle inference errors

### Jinja / Chat Templates
- `jinja`: correct default size for string slices (#19913) — fixes slice operations on short strings

---

## API Changes

### `include/llama.h`
- No changes

### `ggml/include/ggml.h`
- No changes

### `tools/mtmd/mtmd.h` / `tools/mtmd/clip.h`
- No changes

### State Save/Load Behavioral Changes
- No breaking changes to state save/load

---

## Risk Assessment

### LOW: KV Cache M-RoPE can_shift() fix
KV cache shift correctness fix for M-RoPE models (Qwen3VL, Llama4). Existing session cache files for these models may produce slightly different outputs after the fix, but no crash risk.

### LOW: mtmd n_tokens padding fix
Fixes token count padding in vision sequences. Subtle correctness improvement; unlikely to cause visible failures.

### LOW: Jinja string slice fix
Only affects chat templates that perform slice operations on short strings.

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| Vision encoders | All 18 in `tools/mtmd/models/` | All 18 already patched in |

**No structural changes** — build script requires no modifications for this upgrade.

---

## Action Items

1. **REQUIRED**: Rebuild `llama.xcframework` with `build-xcframework-ios.sh`
2. **Recommended**: Test Qwen3VL / Llama4 multimodal inference to verify M-RoPE KV cache fix
3. **Recommended**: Test vision model prompt caching behavior (new server feature)
