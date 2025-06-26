# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and Development Commands

### iOS/macOS XCFramework Build
- Build XCFramework: `./build-xcframework-ios.sh`
- Clean previous builds: `rm -rf build-apple build-ios-sim build-ios-device build-macos`
- Verify framework symbols: `nm -gU build-apple/llama.xcframework/macos-arm64_x86_64/llama.framework/llama | grep <symbol>`

### Standard CMake Build
- Configure: `cmake -B build -DGGML_METAL=ON -DBUILD_SHARED_LIBS=ON`
- Build: `cmake --build build --config Release`
- Run tests: `ctest --test-dir build`
- Clean: `rm -rf build`

### Common Build Options
- Enable Metal (Apple GPU): `-DGGML_METAL=ON`
- Enable CUDA (NVIDIA GPU): `-DGGML_CUDA=ON`
- Enable Vulkan: `-DGGML_VULKAN=ON`
- Build tools: `-DLLAMA_BUILD_TOOLS=ON`
- Build server: `-DLLAMA_BUILD_SERVER=ON`
- Build tests: `-DLLAMA_BUILD_TESTS=ON`

### Running Examples
- Basic inference: `./build/bin/llama-cli -m model.gguf -p "Hello"`
- HTTP server: `./build/bin/llama-server -m model.gguf`
- Multimodal CLI: `./build/bin/llama-mtmd-cli -m model.gguf -p "Describe this image" --image image.jpg`
- Quantize model: `./build/bin/llama-quantize input.gguf output.gguf Q4_K_M`

## Architecture Overview

### Core Components
- **libllama**: Main inference library exposing C API via `include/llama.h`
- **ggml**: Low-level tensor library in `ggml/` submodule providing backend abstractions
- **Multimodal Support**: CLIP vision models and audio processing via files in `tools/mtmd/`
- **Backend Implementations**: CPU, Metal, CUDA, Vulkan, SYCL in `ggml/src/`

### iOS/macOS Integration
The `build-xcframework-ios.sh` script creates an XCFramework by:
1. Copying multimodal files from `tools/mtmd/` to `src/` for inclusion
2. Building static libraries for each platform (iOS device/simulator, macOS)
3. Combining static libraries into dynamic frameworks using libtool
4. Creating proper framework structure with headers, module maps, and Info.plist
5. Generating dSYM debug symbols separately
6. Packaging everything into an XCFramework bundle

Key configuration:
- Minimum OS versions: iOS 16.4, macOS 13.3
- Metal backend enabled with BF16 support
- Tools and examples excluded from iOS builds
- Universal binaries for simulators (arm64 + x86_64)

### File Organization
```
llama.cpp/
├── include/llama.h         # Main C API
├── src/                    # Core implementation
│   ├── llama*.cpp         # Main library source
│   ├── clip.cpp           # CLIP support (copied from tools/mtmd)
│   └── mtmd*.cpp          # Multimodal support (copied from tools/mtmd)
├── ggml/                   # Tensor library submodule
│   ├── include/           # GGML headers
│   └── src/               # Backend implementations
├── tools/                  # Command-line tools
│   ├── main/              # llama-cli
│   ├── server/            # HTTP server
│   └── mtmd/              # Multimodal source files
├── common/                 # Shared utilities
└── build-xcframework-ios.sh # iOS/macOS framework builder
```

### Key APIs
- Model loading: `llama_model_load()`, `llama_model_free()`
- Context creation: `llama_context_new()`, `llama_context_free()`
- Tokenization: `llama_tokenize()`, `llama_token_to_piece()`
- Inference: `llama_decode()`, `llama_get_logits()`
- Multimodal: `llava_load_image()`, `llava_encode_image()`

## Important Notes

- The project uses CMake with minimum version 3.14
- When modifying multimodal support, edit files in `tools/mtmd/` not `src/`
- The XCFramework build automatically handles architecture-specific optimizations
- Metal shaders are embedded in the library (`GGML_METAL_EMBED_LIBRARY=ON`)
- Debug symbols are separated into dSYM files to reduce framework size
- Use absolute paths when building the XCFramework
- The library exposes C-style APIs for maximum compatibility
- Framework includes Swift module maps for direct Swift interoperability