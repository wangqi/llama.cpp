# llama.cpp Upgrade: tag-b8690 -> tag-b8763

## Summary

| Item | Detail |
|------|--------|
| Previous tag | tag-b8690 |
| New tag | tag-b8763 |
| Upgrade date | 2026-04-12 |
| New vision/audio encoders | dotsocr.cpp, gemma4a.cpp, step3vl.cpp |
| Build script patched | Yes |

## New Vision/Audio Encoder Files

Three new files added to `tools/mtmd/models/` requiring `build-xcframework-ios.sh` patches:

| File | Model | PR |
|------|-------|----|
| `dotsocr.cpp` | Dots.OCR OCR vision model | #17575 |
| `gemma4a.cpp` | Gemma 4 audio conformer encoder | #21421 |
| `step3vl.cpp` | Step3-VL-10B vision-language model | #21287 |

## iOS/macOS Relevant Commits

### Vision / Audio
- `mtmd: add Gemma 4 audio conformer encoder support (#21421)` - new audio encoder for Gemma 4 multimodal
- `mtmd : add MERaLiON-2 multimodal audio support (#21756)` - uses conformer encoder (no new file)
- `mtmd: support dots.ocr (#17575)` - new OCR vision encoder
- `model : support step3-vl-10b (#21287)` - new vision-language model
- `model : fix multimodal padding token for gemma3n/gemma4 (#21625)` - multimodal fix

### Metal / ARM64
- `metal : add missing mm-id specializations for q1_0 (#21662)` - Q1_0 Metal backend completeness
- `ggml : fix a few instances of missing GGML_TYPE_Q1_0 cases (#21716)` - Q1_0 type coverage

### Gemma 4
- `common : better align to the updated official gemma4 template (#21704)`
- `common : enable reasoning budget sampler for gemma4 (#21697)`
- `model : make Gemma 4 shared-KV tail attn_k tensors optional on load (#21739)`

### API Changes
- `include/llama.h`: Added `LLAMA_SPLIT_MODE_TENSOR = 3` to `llama_split_mode` enum (experimental tensor parallelism)
- `ggml : backend-agnostic tensor parallelism (experimental) (#19378)`

## Risk Assessment

| Area | Risk | Notes |
|------|------|-------|
| New encoder files | Low | Additive only; existing models unaffected |
| Metal Q1_0 fix | Low | Additive specializations, no behavior change for non-Q1_0 |
| Gemma 4 fixes | Low | Corrective fixes, improves existing support |
| API: LLAMA_SPLIT_MODE_TENSOR | Low | New enum value; not used on iOS |
| Overall | Low | Mostly additive changes; no breaking API changes to llama.h/mtmd.h |
