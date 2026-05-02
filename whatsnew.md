# llama.cpp Upgrade: b8933 → b9008

**Date:** 2026-05-02
**Commits in range:** 76 upstream commits merged

---

## New Features

### New Vision Models
None — all vision encoder `.cpp` files (`qwen3a.cpp`, `yasa2.cpp`, and 25 others) were already present at b8933. No build script changes required.

### New Text Model Architectures
| Model | PR | Notes |
|-------|-----|-------|
| Nemotron Nano 3 Omni | #22481 | GGUF convert support; multimodal omni model |

### Performance — Integer Quantization (i-quants)
- **Fast matmul kernels for i-quants** (#22504): New tile-based matrix multiplication for IQ2/IQ3/IQ4 quantization types — significant throughput improvement on ARM64/Apple Silicon
- **Fast mat-vec kernels for i-quants** (#22344): Complementary vector-matrix kernel for single-token decode — reduces latency for quantized models

### ggml Library
- Bumped to version 0.10.2 (two point releases: 0.10.1 → 0.10.2) with SVE-tuned GEMM kernels and 64-byte aligned tile buffers

### Stability / Correctness
- **Qwen3 + LLaMA** (#22421): Removed duplicate `wo_s` scale in `build_attn` — fixes subtle inference inaccuracy
- **Quantization** (#22572): Fixed `--tensor-type` flag being ignored when a default `qtype` was set
- **llama-mmap** (#22497): Switched to `ftello`/`fseeko` for correct 64-bit file offset on large models

### Server / Tooling
- Checkpoint host copy optimization for faster state save/load (#22558)

---

## API Changes

### `include/llama.h`
- No changes detected between tag-b8933 and tag-b9008.

### `tools/mtmd/mtmd.h` / `tools/mtmd/clip.h`
- No changes detected.

### State Save/Load
- Internal checkpoint memory handling optimized (#22558) — on-disk format unchanged, existing session cache files remain valid.

---

## Risk Assessment

### LOW: Fast i-quant kernel path
New GEMM/GEMV kernels activate for IQ2*/IQ3*/IQ4* models. If a quantized model produces unexpected output, switch to a K-quant variant. Extremely unlikely on Metal.

### LOW: Qwen3/LLaMA wo_s scale fix
Corrects a pre-existing inaccuracy. Generated text may differ slightly from b8933 for Qwen3 and LLaMA families — expected and correct.

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| Vision encoders | Same 27 files | Same 27 files |
| New files | None | None |

**No structural changes needed.**

---

## Action Items

1. **REQUIRED**: Rebuild `llama.cpp.xcframework` with `build-xcframework-ios.sh`
2. **Recommended**: Test Qwen3 and LLaMA output — minor numerical differences from wo_s fix are expected
3. **Recommended**: Benchmark IQ2/IQ3/IQ4 models — throughput improvement expected vs b8933
