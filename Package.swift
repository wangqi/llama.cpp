// swift-tools-version:5.7

import PackageDescription

let package = Package(
    name: "LlamaFramework",
    platforms: [
        .iOS(.v16) // Ensure this matches your deployment target
    ],
    products: [
        .library(
            name: "llama",
            targets: ["LlamaFramework"]
        ),
    ],
    targets: [
        .binaryTarget(
            name: "LlamaFramework",
            path: "build-apple/llama.xcframework"
        ),
    ]
)
