# llama.cpp Upgrade Report: tag-b7845 to tag-b7921

**Upgrade Date:** February 3, 2026
**Previous Version:** tag-b7845
**New Version:** tag-b7921
**Total Commits:** ~76 commits between the tags

---

## Executive Summary

This upgrade brings significant performance improvements for iOS/macOS platforms, particularly in Metal backend optimization, ARM CPU vectorization, and Vulkan backend fixes. No breaking API changes were detected in the public headers.

**Risk Level:** LOW - Mostly performance optimizations and bug fixes with no major API changes

---

## iOS/macOS Specific Changes

### 1. Metal Backend Enhancements

#### 1.1 Metal Virtual Devices Support (`6fdddb498`)
- **Commit:** `metal : support virtual devices (#18919)`
- **Files Modified:**
  - `ggml/src/ggml-metal/ggml-metal-context.h/m`
  - `ggml/src/ggml-metal/ggml-metal-device.h/m`
  - `ggml/src/ggml-metal/ggml-metal.cpp`
- **Changes:**
  - Added support for virtual devices in Metal backend
  - Implemented buffer type context memory management
  - Added events and async tensor copy functionality
- **Impact:** Improved Metal backend flexibility for virtual environments

#### 1.2 Metal Flash Attention Optimization (`c55bce415`)
- **Commit:** `metal : minor cleanup (#19251)`
- **File Modified:** `ggml/src/ggml-metal/ggml-metal-impl.h`
- **Changes:**
  - Minor cleanup and optimization of Flash Attention implementation
  - Modified threadgroup dispatch calculations
- **Impact:** Performance improvements in Metal backend for attention operations

#### 1.3 Metal Resource Location Extension
- **Commit:** Earlier in the update series
- **Changes:**
  - Extended Metal resource file location to search in binary's directory
  - Added support for resolving symbolic links
- **Impact:** Better compatibility with build systems like Bazel and sandboxed environments

---

### 2. ARM/Neon Optimizations

#### 2.1 ARM64 Q4_K Scale Vectorization (`6ad70c5a7`)
- **Commit:** `ggml-cpu: arm64: Q4_K scale unroll and vectorization (#19108)`
- **File Modified:** `ggml/src/ggml-cpu/arch/arm/repack.cpp`
- **Changes:**
  - Optimized Q4_K scale operations with unrolling and vectorization
  - Targeted at ARM64 architecture
- **Impact:** Significant performance improvements for ARM-based iOS/macOS devices (iPhone, iPad, Apple Silicon Mac)

#### 2.2 ARM Build Fix (`9177484`)
- **Commit:** Earlier in the series
- **Changes:**
  - Fixed ARM build issues with `GGML_NATIVE` feature detection
  - Updated CMake configuration for better ARM CPU detection
- **Impact:** Improved build compatibility on ARM platforms

---

### 3. Vulkan Backend (macOS)

#### 3.1 Vulkan Device Deduplication Fix (`88d23ad51`)
- **Commit:** `vulkan: handle device dedup on MacOS + Vega II Duo cards (#19058)`
- **Changes:**
  - Fixed Vulkan device deduplication on macOS
  - Modified device UUID handling to work around MoltenVK limitations
- **Impact:** Better multi-GPU support on macOS with Vulkan backend

---

### 4. Core API and Backend Improvements

#### 4.1 ggml-backend Async Fix (`59377a6c8`)
- **Commit:** `ggml-backend: fix async set/get fallback sync (#19179)`
- **File Modified:** `ggml/src/ggml-backend.cpp`
- **Changes:**
  - Fixed async set/get fallback synchronization
- **Impact:** More reliable async operations in the backend

#### 4.2 ggml-cpu Flash Attention Optimization (`9f682fb64`)
- **Commit:** `ggml-cpu: FA split across kv for faster TG (#19209)`
- **Files Modified:**
  - `ggml/include/ggml-cpu.h`
  - `ggml/src/ggml-cpu/ggml-cpu-impl.h`
- **Changes:**
  - Split Flash Attention across KV for faster token generation
- **Impact:** Improved CPU performance on mobile devices

---

## Multimodal (Vision/Audio) Changes

### 5.1 mtmd Min/Max Pixels Metadata (`07a7412a3`)
- **Commit:** `mtmd: add min/max pixels gguf metadata (#19273)`
- **Files Modified:**
  - `tools/mtmd/clip-impl.h`
  - `tools/mtmd/clip.cpp`
- **Changes:**
  - Added `IMAGE_MIN_PIXELS` and `IMAGE_MAX_PIXELS` metadata keys
  - Extended GGUF metadata for vision models
- **Impact:** Better support for dynamic image sizing in vision models

### 5.2 MiniCPM-o 4.5 Vision Support (`ec6c7421e`)
- **Commit:** `mtmd: support MiniCPM-o 4.5(vision only) (#19211)`
- **Files Modified:**
  - `tools/mtmd/clip.cpp`
  - `tools/mtmd/mtmd.cpp`
  - `tools/mtmd/legacy-models/minicpmv-convert-image-encoder-to-gguf.py`
- **Changes:**
  - Added support for MiniCPM-o 4.5 (vision only)
  - Updated SiglipVisionConfig handling
- **Impact:** Support for newer MiniCPM vision models

---

## General Improvements

### Performance Improvements
- Metal Flash Attention optimizations
- ARM64 Q4_K scale vectorization
- ggml-cpu Flash Attention split optimization
- Vulkan device deduplication fix

### Bug Fixes
- ARM build fixes
- ggml-backend async synchronization
- Vulkan device UUID handling
- Various cleanups and fixes

### New Features
- Metal virtual devices support
- Extended Metal resource location
- Vulkan device deduplication handling

---

## Vision Model Status

### Current Vision Models in tools/mtmd/models/
The following vision encoders are present (no new models added in this upgrade):

| File | Description | Added In |
|------|-------------|----------|
| cogvlm.cpp | CogVLM vision encoder | Pre-b7845 |
| internvl.cpp | InternVL vision encoder | Pre-b7845 |
| kimivl.cpp | KimiVL vision encoder | Pre-b7845 |
| llama4.cpp | LLaMA-4 vision encoder | Pre-b7845 |
| llava.cpp | LLaVA vision encoder | Pre-b7845 |
| minicpmv.cpp | MiniCPM-V vision encoder | Pre-b7845 |
| pixtral.cpp | Pixtral vision encoder | Pre-b7845 |
| qwen2vl.cpp | Qwen2VL vision encoder | Pre-b7845 |
| qwen3vl.cpp | Qwen3VL vision encoder | Pre-b7845 |
| siglip.cpp | SigLIP vision encoder | Pre-b7845 |
| whisper-enc.cpp | Whisper audio encoder | b7610 |
| conformer.cpp | Conformer audio encoder | b7549 |
| glm4v.cpp | GLM-4V vision encoder | b7610 |
| youtuvl.cpp | YouTuVL vision encoder | b7703 |
| mobilenetv5.cpp | MobileNetV5 vision encoder (Gemma3) | b7703 |

**Status:** All existing models are still present. No new vision models added in this upgrade.

---

## Build Script Comparison

### Official build-xcframework.sh Changes
- **Result:** NO CHANGES detected between tag-b7845 and tag-b7921
- The official build script remains identical

### Custom build-xcframework-ios.sh Status
Our custom build script has the following differences from the official:

| Feature | Official | Custom | Status |
|---------|----------|--------|--------|
| Optimization flags | No `-O3` | `-O3 -fno-finite-math-only` | OK |
| Vision model support | Basic | Extended with all clip-models | OK |
| Mac Catalyst support | No | Yes (full implementation) | OK |
| VisionOS/tvOS builds | Yes | Commented out | OK (not needed) |
| HTTP library flag | `-DLLAMA_OPENSSL=OFF` | `-DLLAMA_HTTPLIB=OFF` | OK (both work) |

**Conclusion:** Our custom build script is up-to-date and includes all necessary vision model files. No changes required.

---

## API Changes

### Public Headers Analysis
- **llama.h:** No breaking changes detected
- **ggml.h:** No breaking changes detected
- **ggml-backend.h:** No breaking changes detected
- **ggml-metal.h:** No breaking changes detected
- **clip.h:** Minor additions (min/max pixels metadata)
- **mtmd.h:** No breaking changes detected

### Compatibility
- **Source Compatibility:** 100% - All existing API calls remain valid
- **Binary Compatibility:** Requires rebuild of the xcframework due to internal changes

---

## Risk Assessment

### LOW Risk Items
1. **Metal Virtual Devices** - New feature, doesn't affect existing code paths
2. **ARM Optimizations** - Performance improvements, no behavior changes
3. **Vision Model Metadata** - Additive only, backward compatible

### MEDIUM Risk Items
1. **ggml-backend Async Fix** - Changes synchronization behavior, but fixes bugs
2. **Vulkan Device Deduplication** - Platform-specific fix for macOS

### HIGH Risk Items
- **NONE** - No high-risk changes detected

---

## Testing Recommendations

### Priority 1 (Must Test)
1. **Metal Backend Functionality**
   - Verify inference on iOS device (Metal backend)
   - Verify inference on macOS (Apple Silicon)
   - Test with various model sizes (7B, 13B, 30B+)

2. **Vision Model Loading**
   - Test all supported vision encoders (LLaVA, Qwen2VL, etc.)
   - Verify multimodal inference works correctly

3. **ARM CPU Fallback**
   - Test when Metal backend is not available
   - Verify Q4_K quantized models work correctly

### Priority 2 (Should Test)
1. **Vulkan Backend** (if used in macOS build)
2. **Async Operations**
3. **Memory Usage**

### Priority 3 (Nice to Test)
1. **Performance Benchmarks**
   - Compare inference speed before/after upgrade
   - Measure memory consumption
2. **Edge Cases**
   - Large batch sizes
   - Very long contexts

---

## Migration Steps

### 1. Update Submodule
```bash
cd thirdparty/llama.cpp
git fetch origin
git checkout tag-b7921
cd ../..
git add thirdparty/llama.cpp
git commit -m "Upgrade llama.cpp from tag-b7845 to tag-b7921"
```

### 2. Rebuild XCFramework
```bash
cd thirdparty/llama.cpp
rm -rf build-apple build-ios-sim build-ios-device build-macos
./build-xcframework-ios.sh
```

### 3. Verify Framework
```bash
# Check framework was created successfully
ls -la build-apple/llama.xcframework/

# Verify symbols
nm -gU build-apple/llama.xcframework/ios-arm64/llama.framework/llama | grep llama_
nm -gU build-apple/llama.xcframework/macos-arm64_x86_64/llama.framework/llama | grep llama_
```

### 4. Update Project
```bash
# Copy new framework to Xcode project
cp -R build-apple/llama.xcframework /path/to/Xcode/project/
```

### 5. Test Build
```bash
# Clean build
rm -rf ~/Library/Developer/Xcode/DerivedData/AIAssistant-*

# Build for iOS
xcodebuild -project AIAssistant.xcodeproj -scheme AIAssistant \
    -destination "platform=iOS Simulator,name=iPhone 16 Pro"

# Build for macOS
xcodebuild -project AIAssistant.xcodeproj -scheme AIAssistantMac \
    -destination "platform=macOS"
```

---

## Build Script Status

### No Changes Required
The custom `build-xcframework-ios.sh` script does NOT need any changes for this upgrade:

1. **All vision model files** are already included in the `copy_mtmd_files()` function
2. **CMake flags** remain compatible
3. **Patch logic** for CMakeLists.txt still works correctly
4. **Framework structure** remains unchanged

### Verification
```bash
# Check that all vision model files are present
ls tools/mtmd/models/*.cpp | wc -l  # Should be 15 files

# Check that build script has them all
grep "cp -fp.*models/.*cpp" build-xcframework-ios.sh | wc -l  # Should match
```

---

## Conclusion

This upgrade to tag-b7921 is a **low-risk, high-reward** update focused on performance improvements for iOS and macOS platforms. The changes primarily benefit:

1. **Metal Backend** - Virtual devices support and Flash Attention optimizations
2. **ARM CPU** - Vectorized Q4_K operations for better performance
3. **Vulkan Backend** - Better multi-GPU support on macOS
4. **Vision Models** - Extended metadata support for newer models

No breaking API changes were detected, and the build script requires no modifications. The upgrade is recommended for all users targeting iOS and macOS platforms.

---

## References

- **Commit Range:** tag-b7845 to tag-b7921
- **Total Commits:** ~76 commits
- **Key Commits:**
  - `6fdddb498` - Metal virtual devices
  - `6ad70c5a7` - ARM64 Q4_K vectorization
  - `88d23ad51` - Vulkan device deduplication
  - `59377a6c8` - ggml-backend async fix
  - `07a7412a3` - mtmd min/max pixels metadata
  - `ec6c7421e` - MiniCPM-o 4.5 support

---

*Document Generated: February 3, 2026*
*llama.cpp Version: tag-b7921*
