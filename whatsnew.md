# llama.cpp Upgrade: b9008 → b9090

**Date:** 2026-05-09
**Commits in range:** 87 upstream commits merged

---

## New Features

### New Vision/Audio Encoders
- **granite-speech.cpp** — IBM Granite 4.0 Speech audio encoder (ibm-granite/granite-4.0-1b-speech). Added to `src/clip-models/` via `copy_mtmd_files()` in `build-xcframework-ios.sh`.
- **MiniCPM-V 4.6** — mtmd support added (#22529)

### New Text Model Architectures
| Model | PR | Notes |
|-------|-----|-------|
| MiMo v2.5 | #22493 | Reasoning model; also adds flash attention MMA/Tiles path (#22812) |
| Sarashina2.2 Vision 3B | #22103 | Japanese vision-language model |
| Sarvam MoE | #20275 | Indian language MoE architecture |
| Gemma4 26B A4B NVFP4 | #22804 | NVFP4 quant variant |

### ggml v0.11.0
- Bumped from 0.10.2 to 0.11.0
- CPU: fused RMS_NORM + MUL kernel (#22423) — fewer passes over tensors in every transformer layer
- Fast Walsh-Hadamard transform for KV rotation (#22631)

### KleidiAI v1.24.0
Updated from prior version; ARM64/Apple Silicon matrix kernel improvements (#22549).

### Server / API Enhancements
- Vertex AI compatible API support (#22545)
- `/models?reload=1` endpoint (#21848)
- `get_datetime` server tool (#22649)

---

## API Changes

### `include/llama.h`
- **Added**: `LLAMA_STATE_SEQ_FLAGS_ON_DEVICE 2` — keeps tensor data in device buffers for faster save/load; existing callers passing `0` are unaffected.

### `tools/mtmd/mtmd.h` / `tools/mtmd/clip.h`
- No breaking changes detected.

### State Save/Load Behavioral Changes
- New `LLAMA_STATE_SEQ_FLAGS_ON_DEVICE` flag allows device-resident KV cache snapshots. Not used by our app currently; existing session files remain compatible.

---

## Risk Assessment

### LOW: New audio encoder granite-speech.cpp
Added to clip-models/ GLOB. Automatically compiled into the xcframework. No linker risk since CMakeLists uses `file(GLOB ...)` since b8843.

### LOW: ggml 0.11.0 CPU fused kernels
RMS_NORM+MUL fusion is a drop-in optimization; no API changes, no correctness risk on Apple Silicon Metal path.

### LOW: MiMo v2.5 flash attention MMA/Tiles
New codepath behind model-specific dispatch in the attention builder. Does not affect existing models.

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| clip-models/ | Uses GLOB | Uses GLOB (since b8843) |
| New encoder | granite-speech.cpp present | Added in this upgrade |

**No structural changes required** to the build script beyond the new `cp -fp` line.

---

## Action Items

1. **REQUIRED**: Rebuild xcframework with `./build-xcframework-ios.sh` to include granite-speech.cpp.
2. **Recommended**: Smoke-test a local llama/Qwen model load to verify no regression from ggml v0.11.0.
3. **No session cache invalidation needed**: `LLAMA_STATE_SEQ_FLAGS_ON_DEVICE` is not used by the app; existing `.llama_session` files remain valid.
