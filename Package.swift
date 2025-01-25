// swift-tools-version:5.5

import PackageDescription

// MARK: - Source Files Configuration
var sources = [
    // Core llama.cpp files
    "src/llama.cpp",
    "src/llama-adapter.cpp",
    "src/llama-arch.cpp",
    "src/llama-batch.cpp",
    "src/llama-chat.cpp",
    "src/llama-context.cpp",
    "src/llama-cparams.cpp",
    "src/llama-grammar.cpp",
    "src/llama-hparams.cpp",
    "src/llama-impl.cpp",
    "src/llama-kv-cache.cpp",
    "src/llama-mmap.cpp",
    "src/llama-model-loader.cpp",
    "src/llama-model.cpp",
    "src/llama-quant.cpp",
    "src/llama-sampling.cpp",
    "src/llama-vocab.cpp",
    "src/unicode.cpp",
    "src/unicode-data.cpp",

    "common/build-info.cpp",
    "common/common.cpp",
    "common/sampling.cpp",
    "common/json-schema-to-grammar.cpp",
    "common/log.cpp",
    "common/arg.cpp",

    // GGML core files
    "ggml/src/ggml.c",
    "ggml/src/ggml-alloc.c",
    "ggml/src/ggml-backend.cpp",
    "ggml/src/ggml-backend-reg.cpp",
    "ggml/src/ggml-quants.c",
    "ggml/src/ggml-threading.cpp",
    "ggml/src/ggml-metal/ggml-metal.m",
    "ggml/src/ggml-blas/ggml-blas.cpp",
    
    // CPU-specific implementations
    "ggml/src/ggml-cpu/ggml-cpu.c",
    "ggml/src/ggml-cpu/ggml-cpu.cpp",
    "ggml/src/ggml-cpu/ggml-cpu-aarch64.cpp",
    "ggml/src/ggml-cpu/ggml-cpu-hbm.cpp",
    "ggml/src/ggml-cpu/ggml-cpu-quants.c",
    "ggml/src/ggml-cpu/ggml-cpu-traits.cpp",
    "ggml/src/ggml-cpu/llamafile/sgemm.cpp",
    "ggml/src/gguf.cpp",
    
    //Lava
    "examples/llava/llava.cpp",
    "examples/llava/clip.cpp",
    "examples/llava/llava-cli.cpp",
    
]

// MARK: - Build Settings
var resources: [Resource] = []
var linkerSettings: [LinkerSetting] = []
var cSettings: [CSetting] = [
    // Optimization and warning settings
    .unsafeFlags(["-Wno-shorten-64-to-32", "-O3", "-DNDEBUG"]),
    .unsafeFlags(["-fno-objc-arc"]),
    
    // Header search paths
    .headerSearchPath("include"),
    .headerSearchPath("ggml/include"),
    .headerSearchPath("ggml/src"),
    .headerSearchPath("ggml/src/ggml-cpu"),
    .headerSearchPath("src"),
    .headerSearchPath("common"),
    
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

// MARK: - Platform Specific Configuration
#if canImport(Darwin)
// Add Metal support for Apple platforms
sources.append("ggml/src/ggml-metal/ggml-metal.m")
resources.append(.process("ggml/src/ggml-metal/ggml-metal.metal"))
linkerSettings.append(contentsOf: [
    .linkedFramework("Metal"),
    .linkedFramework("MetalKit"),
    .linkedFramework("MetalPerformanceShaders"),
    .linkedFramework("Accelerate"),
    .linkedFramework("Foundation")
])
cSettings.append(contentsOf: [
    // These placeholders get replaced by our script:
    .define("DEFAULT_LLAMA_BUILD_NUMBER", to: "$LLAMA_BUILD_NUMBER_PLACEHOLDER"),
    .define("DEFAULT_LLAMA_COMMIT",       to: "\"$LLAMA_COMMIT_PLACEHOLDER\""),
    .define("DEFAULT_LLAMA_COMPILER",     to: "\"$LLAMA_COMPILER_PLACEHOLDER\""),
    .define("DEFAULT_LLAMA_BUILD_TARGET", to: "\"$LLAMA_BUILD_TARGET_PLACEHOLDER\""),
])
#endif

#if os(Linux)
cSettings.append(.define("_GNU_SOURCE"))
#endif

// MARK: - Package Definition
let package = Package(
    name: "llama",
    platforms: [
        .macOS(.v12),
        .iOS(.v15)
    ],
    products: [
        .library(
            name: "llama",
            targets: ["llama"])
    ],
    targets: [
        .target(
            name: "llama",
            path: ".",
            exclude: [
                "build",
                "cmake",
                "examples",
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
            publicHeadersPath: "spm-headers",  // Changed from spm-headers to include for consistency
            cSettings: cSettings,
            linkerSettings: linkerSettings
        )
    ],
    cxxLanguageStandard: .cxx17
)