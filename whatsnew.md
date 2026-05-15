# llama.cpp Upgrade: b9090 → b9165

**Date:** 2026-05-15
**Commits in range:** ~75 upstream commits merged

---

## New Features

### New Vision Models
- **MiMo v2.5 Vision** (`mimovl.cpp`) — multimodal encoder for MiMo-series vision-language models; added to `src/clip-models/` in build script

### New Text Model Architectures
No new text-only model architectures in this range.

### Metal Performance
- `metal : promote mul_mv/mul_mm batch divisors to function constants` (#22711) — batch divisor values promoted to Metal function constants, reducing pipeline state overhead for matrix-vector and matrix-matrix multiply on Apple Silicon

### Multimodal Enhancements
- `mtmd, server, common: expose modalities to /v1/models` (#22952) — mmproj modality flags (vision/audio) now surfaced via the `/v1/models` endpoint; lightweight capability probe via new `mtmd_get_cap_from_file()` API

### Sampling Improvements
- `backend sampling: support returning post-sampling probs` (#22622) — backend samplers can now return post-sampling token probabilities, enabling richer sampling diagnostics

### Server Enhancements
- `server, webui: support continue generation on reasoning models` (#22727)
- `server, webui: accept continue_final_message flag for vLLM API compat` (#23012)

### Third-Party Library Updates
- cpp-httplib updated to 0.44.0 (#22919)

---

## API Changes

### `include/llama.h`
- **Added**: `LLAMA_STATE_SEQ_FLAGS_NONE 0` — zero-value flag constant for sequence state operations; additive, no action required

### `tools/mtmd/clip.h`
- **Added**: `struct clip_cap { bool has_vision; bool has_audio; }` and `clip_get_cap(const char * fname)` — lightweight mmproj capability probe without full context init; additive

### `tools/mtmd/mtmd.h`
- **Added**: `struct mtmd_caps { bool inp_vision; bool inp_audio; }` and `MTMD_API struct mtmd_caps mtmd_get_cap_from_file(const char * mmproj_fname)` — high-level wrapper matching `clip_get_cap`; marked EXPERIMENTAL, breaking changes expected; additive for now

### State Save/Load Behavioral Changes
- No behavioral changes to `llama_state_save_file` / `llama_state_load_file` detected.

---

## Risk Assessment

### LOW: MiMo v2.5 Vision encoder (mimovl.cpp)
New encoder file added to build script. Glob in CMakeLists.txt picks it up automatically. No action beyond the `cp -fp` line already added.

### LOW: Metal batch divisor function constants
Purely additive Metal optimization; no API or behavioral change.

### LOW: `mtmd_get_cap_from_file` marked EXPERIMENTAL
New function, not called by our Swift bridge. No immediate action required; watch for breaking changes in future upgrades.

### LOW: Sampling post-sampling probs
Additive to backend samplers; our Swift bridge does not use the new output field.

---

## Build Script Comparison

| Aspect | Official `build-xcframework.sh` | Our `build-xcframework-ios.sh` |
|--------|--------------------------------|-------------------------------|
| Platforms | iOS, macOS, visionOS, tvOS | iOS, macOS, Mac Catalyst only |
| Vision encoders | Glob via CMakeLists | Explicit cp + glob (same result) |
| New file: mimovl.cpp | Included via glob | Added explicit `cp -fp` line 2026-05-15 |

**No structural changes** required to our build script beyond the `mimovl.cpp` copy line.

---

## Action Items

1. **REQUIRED**: Rebuild the XCFramework to include `mimovl.cpp` (MiMo v2.5 Vision).
2. **Recommended**: Test an existing vision model (e.g., Qwen3 VL) after rebuild to confirm no regressions.
3. **Watch**: `mtmd_get_cap_from_file` is marked EXPERIMENTAL — monitor for breaking changes in the next upgrade.
