#!/usr/bin/env bash
# build_package.sh
#
# Purpose:
#  1) Gather build info (Git commit count/commit hash, compiler name, etc.).
#  2) Substitute placeholders in Package.swift (or a temporary copy).
#  3) Invoke xcodebuild to build the Swift package.
#
# Usage:
#   ./build_package.sh
#
# Adjust variables and xcodebuild arguments as needed.

echo "Build llama.cpp with cmake first to prepare required generated files"
cmake -B build
cd build
cmake -G Xcode .. \
    -DGGML_METAL_USE_BF16=ON \
    -DGGML_METAL_EMBED_LIBRARY=ON \
    -DLLAMA_BUILD_EXAMPLES=OFF \
    -DLLAMA_BUILD_TESTS=OFF \
    -DLLAMA_BUILD_SERVER=OFF \
    -DBUILD_SHARED_LIBS=ON \
    -DCMAKE_FRAMEWORK=ON \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"

echo "cmake can use $(sysctl -n hw.logicalcpu) multiple jobs in parallel"
cmake --build . --config Release -j $(sysctl -n hw.logicalcpu)

echo "Install the generated binaries & libraries to /usr/local/lib and /usr/local/include"
echo "sudo cmake --install . --config Release"

# build for swift package
cd ..

set -euo pipefail

############################################
# 1. Gather Build Info
############################################

# Get the commit count from Git
BUILD_NUMBER="$(git rev-list HEAD --count || echo 0)"

# Get the current short commit hash
COMMIT_HASH="$(git rev-parse --short HEAD || echo unknown)"

# Optionally detect the compiler from environment or from `clang --version`
# For example:
if command -v clang >/dev/null 2>&1; then
  COMPILER="$(clang --version | head -n 1 | sed 's/ version/ /')"
else
  COMPILER="unknown-compiler"
fi

# Decide on build target (e.g. "macOS", "iOS", "Linux", etc.).
# Or get from an environment variable. We'll default to macOS here.
BUILD_TARGET="${BUILD_TARGET:-iOS}"

echo "====================================="
echo "BUILD_NUMBER: $BUILD_NUMBER"
echo "COMMIT_HASH: $COMMIT_HASH"
echo "COMPILER: $COMPILER"
echo "BUILD_TARGET: $BUILD_TARGET"
echo "====================================="


############################################
# 2. Replace Placeholders in Package.swift
############################################

# We do not want to permanently modify Package.swift in version control, so
# create a temporary copy. Alternatively, you can directly edit `Package.swift`
# and revert changes afterwards.

TMP_PKG_FILE="Package.generated.swift"

# Copy the original to a temporary file
cp Package.swift "$TMP_PKG_FILE"

# Now do in-place replacements of placeholders with real values.
# Using 'sed' with extended regex is typical.  Mac/BSD sed requires a suffix for -i.
sed -i.bak \
  -e "s/\$LLAMA_BUILD_NUMBER_PLACEHOLDER/${BUILD_NUMBER}/g" \
  -e "s/\$LLAMA_COMMIT_PLACEHOLDER/${COMMIT_HASH}/g" \
  -e "s/\$LLAMA_COMPILER_PLACEHOLDER/${COMPILER}/g" \
  -e "s/\$LLAMA_BUILD_TARGET_PLACEHOLDER/${BUILD_TARGET}/g" \
  "$TMP_PKG_FILE"

rm -f "$TMP_PKG_FILE.bak"


############################################
# 3. Build with Xcode Command-Line Tools
############################################

# This assumes your Swift package is at the same directory level as the script.
# "xcodebuild -scheme" requires that your SwiftPM package is opened as an
# Xcode project or workspace. If you only have a Package.swift, you can use
# "xcodebuild -project" with `swift package generate-xcodeproj`, or
# you can use SwiftPM directly with "swift build --package-path ."

# Example: build a Swift package using xcodebuild
# (NOTE: This effectively invokes the same build as in Xcode’s UI, but check your scheme name)

# If you do not have an Xcode project or scheme, you can skip xcodebuild and do:
#   swift build --package-path . --configuration release
# For demonstration, we'll assume you have an Xcode scheme named "llama":
SCHEME_NAME="llama"

echo "Building with xcodebuild (scheme: $SCHEME_NAME) using $TMP_PKG_FILE"

# xcodebuild can be pointed at a particular package file via -xcconfig or
# via environment variables, but it does not have a built-in “use this
# alternate Package.swift” parameter. One workaround:
#
#   1) Temporarily rename Package.generated.swift -> Package.swift
#   2) Build
#   3) Rename back
#
# Or, generate an Xcode project from the custom Package.swift:
#
#   swift package --package-path . generate-xcodeproj --output llama.xcodeproj
#   xcodebuild -project llama.xcodeproj ...

# Let's do the rename approach:
mv Package.swift Package.swift.original
mv "$TMP_PKG_FILE" Package.swift

# Now call xcodebuild
set +e
xcodebuild \
  -scheme "$SCHEME_NAME" \
  -configuration Release \
  -destination 'generic/platform=iOS' \
  build
XCODE_EXIT=$?
set -e

# Restore the original Package.swift
mv Package.swift Package.generated.last
mv Package.swift.original Package.swift

# If you want to keep the generated version for debugging, you can rename or remove it
# rm Package.generated.last

exit $XCODE_EXIT
