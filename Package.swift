// swift-tools-version:5.5

import PackageDescription

// MARK: - Source file grouping
// SwiftPM will choose the compiler based on file extension:
//   .c   => C
//   .cpp => C++
//   .m   => Objective-C
//   .mm  => Objective-C++

let sources = [
    "ggml/src/ggml.c",
    "ggml/src/gguf.cpp",
    "ggml/src/ggml-quants.c",
    "ggml/src/ggml-alloc.c",
    "ggml/src/ggml-backend.cpp",
    "ggml/src/ggml-threading.cpp",
    "ggml/src/ggml-backend-reg.cpp",
    "ggml/src/ggml-metal/ggml-metal.m",
    "ggml/src/ggml-blas/ggml-blas.cpp",
    "ggml/src/ggml-aarch64.c",

    "ggml/src/ggml-cpu/ggml-cpu-aarch64.c",
    "ggml/src/ggml-cpu/ggml-cpu.c",
    "ggml/src/ggml-cpu/ggml-cpu.cpp",
    "ggml/src/ggml-cpu/ggml-cpu-quants.c",
    "ggml/src/ggml-cpu/ggml-cpu-traits.cpp",
    "ggml/src/ggml-cpu/llamafile/sgemm.cpp",
    "src/llama.cpp",
    "src/unicode.cpp",
    "src/unicode-data.cpp",
    "src/llama-grammar.cpp",
    "src/llama-vocab.cpp",
    "src/llama-sampling.cpp",
    "src/llama-context.cpp",
    "src/llama-kv-cache.cpp",
    "src/llama-mmap.cpp",
    "src/llama-quant.cpp",
    "src/llama-model.cpp",
    "src/llama-model-loader.cpp",
    "src/llama-impl.cpp",
    "src/llama-cparams.cpp",
    "src/llama-hparams.cpp",
    "src/llama-chat.cpp",
    "src/llama-batch.cpp",
    "src/llama-arch.cpp",
    "src/llama-adapter.cpp",
    
    "common/build-info.cpp",
    "common/common.cpp",
    "common/log.cpp",
    "common/arg.cpp",
    "common/json-schema-to-grammar.cpp",
    "common/sampling.cpp",
    
    // "common/train.cpp",
    "examples/llava/llava.cpp",
    "examples/llava/clip.cpp",
    "examples/llava/llava-cli.cpp",
]

// MARK: - Build Settings
// These cSettings apply to all C-family source files (C & Objective-C).
var cSettings: [CSetting] = [
    // Optimization and warning settings
    .unsafeFlags(["-fno-objc-arc"]),
    .unsafeFlags(["-Ofast"], .when(configuration: .release)),
    .unsafeFlags(["-O3"], .when(configuration: .debug)),
    .unsafeFlags(["-mfma","-mfma","-mavx","-mavx2","-mf16c","-msse3","-mssse3"]), //for Intel CPU
    .unsafeFlags(["-pthread"]),
    .unsafeFlags(["-fno-objc-arc"]),
    .unsafeFlags(["-Wno-shorten-64-to-32"]),
    .unsafeFlags(["-fno-finite-math-only"], .when(configuration: .release)),
    .unsafeFlags(["-w"]),    // ignore all warnings
    
    // Header search paths
    .headerSearchPath("include"),
    .headerSearchPath("ggml/include"),
    .headerSearchPath("ggml/src"),
    .headerSearchPath("ggml/src/ggml-cpu"),
    .headerSearchPath("ggml/src/ggml-metal"),
    .headerSearchPath("src"),
    .headerSearchPath("common"),
    .headerSearchPath("examples/llava"),

    // Feature flags
    .define("SWIFT_PACKAGE"),
    .define("GGML_USE_ACCELERATE"),
    .define("GGML_BLAS_USE_ACCELERATE"),
    .define("ACCELERATE_NEW_LAPACK"),
    .define("ACCELERATE_LAPACK_ILP64"),
    .define("GGML_USE_BLAS"),
    .define("GGML_USE_LLAMAFILE"),
    .define("GGML_USE_CPU"),
    .define("NDEBUG"),
    .define("GGML_USE_METAL"),
]

// Resources (e.g. Metal shaders)
var resources: [Resource] = []
var linkerSettings: [LinkerSetting] = []

#if canImport(Darwin)
resources.append(.process("ggml/src/ggml-metal/ggml-metal.metal"))
linkerSettings.append(contentsOf: [
    .linkedFramework("Metal"),
    .linkedFramework("MetalKit"),
    .linkedFramework("MetalPerformanceShaders"),
    .linkedFramework("Accelerate"),
    .linkedFramework("Foundation"),
])
cSettings.append(contentsOf: [
    // Build-number placeholders if needed:
    .define("DEFAULT_LLAMA_BUILD_NUMBER", to: "$LLAMA_BUILD_NUMBER_PLACEHOLDER"),
    .define("DEFAULT_LLAMA_COMMIT",       to: "\"$LLAMA_COMMIT_PLACEHOLDER\""),
    .define("DEFAULT_LLAMA_COMPILER",     to: "\"$LLAMA_COMPILER_PLACEHOLDER\""),
    .define("DEFAULT_LLAMA_BUILD_TARGET", to: "\"$LLAMA_BUILD_TARGET_PLACEHOLDER\""),
])
#endif

#if os(Linux)
cSettings.append(.define("_GNU_SOURCE"))
#endif

let package = Package(
    name: "llama",
    platforms: [
        .macOS(.v12),
        .iOS(.v15)
    ],
    products: [
        .library(name: "llama", targets: ["llama"])
    ],
    targets: [
        .target(
            name: "llama",
            path: ".",
            exclude: [
                "build",
                "cmake",
                "examples/batched.swift",
                "examples/convert-llama2c-to-ggml",
                "examples/embedding",
                "examples/infill",
                "examples/llama.swiftui",
                "examples/main",
                "examples/perplexity",
                "examples/quantize",
                "examples/quantize-stats",
                "examples/save-load-state",
                "examples/server",
                "examples/simple",
                "examples/tokenize",
                "scripts",
                "models",
                "tests",
                "pocs",
                ".github",
                ".git",
                "docs",
                "CMakeLists.txt",
                "Makefile"
            ],
            sources: sources,
            resources: resources,
            publicHeadersPath: "spm-headers",   // your public headers go here
            cSettings: cSettings,
            linkerSettings: linkerSettings
        )
    ],
    cxxLanguageStandard: .cxx17
)
