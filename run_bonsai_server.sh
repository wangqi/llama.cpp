#!/usr/bin/env bash
#
# Serve Ternary-Bonsai-2-27B (PrismML PQ2_0 / PTQ1_0) over llama-server's
# OpenAI-compatible API, so any OpenAI-protocol client can talk to it.
#
# The model only runs on our patched fork: it uses ggml type 142 (PQ2_0) and carries
# prism.hadamard.* activation-rotation metadata. A stock llama.cpp build rejects the file.
# See ./prismml_merge.md (next to this script) and, in the app repo,
# helper/docs/llama_cpp_prism.md.
#
# Usage:
#   ./run_bonsai_server.sh                      # PQ2_0, 8K context, port 8080
#   ./run_bonsai_server.sh --ctx 16384
#   ./run_bonsai_server.sh --host 127.0.0.1        # loopback only
#   ./run_bonsai_server.sh --api-key secret123     # require auth (no auth by default)
#   ./run_bonsai_server.sh --model /path/to/other.gguf
#   ./run_bonsai_server.sh --no-think           # disable thinking (plain content only)
#   ./run_bonsai_server.sh --rebuild            # force a fresh llama-server build
#
# wangqi 2026-09-18

set -euo pipefail

# This script lives at the root of the llama.cpp submodule, so its own directory is the
# source tree. Resolving from BASH_SOURCE keeps it working from any cwd.
LLAMA_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$LLAMA_DIR/build-server"
SERVER_BIN="$BUILD_DIR/bin/llama-server"

# ---- defaults ---------------------------------------------------------------

MODEL="${BONSAI_MODEL:-/Volumes/ssd2t/models/Ternary-Bonsai-2-27B-PQ2_0.gguf}"

# The model's own default n_ctx is 262144. Serving at that width tries to allocate a
# ~24 GB shape (measured) and will thrash or die on most machines, so we ALWAYS pass -c
# explicitly. 8192 with a q8_0 KV cache measured 7.85 GB peak RSS on an M-series host.
CTX="${BONSAI_CTX:-8192}"
KV_TYPE="${BONSAI_KV:-q8_0}"
NGL="${BONSAI_NGL:-99}"
# Listen on all interfaces so other machines/devices on the LAN can connect. Note that
# llama-server has NO authentication unless --api-key is passed, so anything that can reach
# this port can use the model. Bind to loopback only with: --host 127.0.0.1
HOST="${BONSAI_HOST:-0.0.0.0}"
API_KEY="${BONSAI_API_KEY:-}"
PORT="${BONSAI_PORT:-8080}"
ALIAS="${BONSAI_ALIAS:-ternary-bonsai-2-27b}"
PARALLEL="${BONSAI_PARALLEL:-1}"
USE_JINJA=1
THINKING=1
FORCE_REBUILD=0

# ---- args -------------------------------------------------------------------

# Print the header comment block (everything after the shebang up to the first blank
# non-comment line), with the leading '# ' stripped.
usage() {
    awk 'NR==1 {next} /^#/ {sub(/^# ?/, ""); print; next} {exit}' "${BASH_SOURCE[0]}"
    exit 0
}

while [ $# -gt 0 ]; do
    case "$1" in
        --model)     MODEL="$2"; shift 2 ;;
        --ctx|-c)    CTX="$2"; shift 2 ;;
        --kv)        KV_TYPE="$2"; shift 2 ;;
        --ngl)       NGL="$2"; shift 2 ;;
        --host)      HOST="$2"; shift 2 ;;
        --port|-p)   PORT="$2"; shift 2 ;;
        --alias)     ALIAS="$2"; shift 2 ;;
        --api-key)   API_KEY="$2"; shift 2 ;;
        --parallel)  PARALLEL="$2"; shift 2 ;;
        --no-jinja)  USE_JINJA=0; shift ;;
        --no-think)  THINKING=0; shift ;;
        --rebuild)   FORCE_REBUILD=1; shift ;;
        -h|--help)   usage ;;
        *) echo "unknown argument: $1 (try --help)" >&2; exit 2 ;;
    esac
done

# ---- preflight --------------------------------------------------------------

if [ ! -f "$MODEL" ]; then
    echo "error: model not found: $MODEL" >&2
    echo >&2
    echo "Fetch it with:" >&2
    echo "  hf download prism-ml/Ternary-Bonsai-2-27B-gguf \\" >&2
    echo "     Ternary-Bonsai-2-27B-PQ2_0.gguf --local-dir /Volumes/ssd2t/models" >&2
    echo >&2
    echo "If HuggingFace Xet stalls on it (it has: GBs transferred, 0 completed)," >&2
    echo "retry with HF_HUB_DISABLE_XET=1, or plain 'curl -L -C -'." >&2
    exit 1
fi

case "$MODEL" in
    *PTQ1_0*) echo "note: PTQ1_0 support is carried but has never been exercised end to end." ;;
esac

# ---- build llama-server if missing or older than the submodule HEAD ----------

needs_build=0
if [ "$FORCE_REBUILD" = "1" ]; then
    needs_build=1
elif [ ! -x "$SERVER_BIN" ]; then
    needs_build=1
else
    # A binary older than the current submodule commit predates the PrismML picks and
    # will reject the file with "unknown type 142". Compare against HEAD's commit time.
    head_epoch="$(git -C "$LLAMA_DIR" log -1 --format=%ct)"
    bin_epoch="$(stat -f %m "$SERVER_BIN")"
    if [ "$bin_epoch" -lt "$head_epoch" ]; then needs_build=1; fi
fi

if [ "$needs_build" = "1" ]; then
    echo "building llama-server from $(git -C "$LLAMA_DIR" log -1 --format='%h %s' | cut -c1-70) ..."
    echo "(first build takes a few minutes; later runs reuse $BUILD_DIR)"

    # The fork compiles the mtmd/clip sources straight into libllama, and they are
    # gitignored copies that a fresh checkout does not have. Reuse the build script's
    # own copy step rather than duplicating the file list here.
    ( cd "$LLAMA_DIR" \
      && sed -n '/^copy_mtmd_files() {/,/^}/p' build-xcframework-ios.sh > /tmp/_copy_mtmd.sh \
      && echo 'copy_mtmd_files' >> /tmp/_copy_mtmd.sh \
      && bash /tmp/_copy_mtmd.sh >/dev/null 2>&1 || true )

    cmake -S "$LLAMA_DIR" -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DGGML_METAL=ON -DGGML_METAL_EMBED_LIBRARY=ON \
        -DLLAMA_BUILD_SERVER=ON -DLLAMA_BUILD_TOOLS=ON \
        -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_EXAMPLES=OFF \
        > "$BUILD_DIR.cmake.log" 2>&1 \
        || { echo "cmake configure failed - see $BUILD_DIR.cmake.log" >&2; exit 1; }

    cmake --build "$BUILD_DIR" -j"$(sysctl -n hw.ncpu)" --target llama-server \
        > "$BUILD_DIR.build.log" 2>&1 \
        || { echo "build failed - see $BUILD_DIR.build.log" >&2
             grep -E "error:" "$BUILD_DIR.build.log" | head -20 >&2; exit 1; }

    echo "built: $SERVER_BIN"
fi

# ---- run --------------------------------------------------------------------

args=(
    -m "$MODEL"
    -c "$CTX"
    -ctk "$KV_TYPE" -ctv "$KV_TYPE"
    -ngl "$NGL"
    --host "$HOST" --port "$PORT"
    --alias "$ALIAS"
    --parallel "$PARALLEL"
)
# --jinja makes the server use the model's own chat template and parse its tool calls,
# instead of a generic built-in template. Keep it on for tool calling.
if [ "$USE_JINJA" = "1" ]; then args+=(--jinja); fi

# This model thinks before answering, and llama-server reports that separately as
# `message.reasoning_content` (and as reasoning_content deltas when streaming) rather than
# in `message.content`. Most OpenAI clients ignore the extra field, but one that renders
# only `content` will look like it is producing nothing until the thinking ends.
# --no-think turns thinking off entirely, so everything arrives in `content`.
if [ "$THINKING" = "0" ]; then args+=(--reasoning off); fi
if [ -n "$API_KEY" ]; then args+=(--api-key "$API_KEY"); fi

# 0.0.0.0 is not an address a client can dial, so show the LAN address for clients and
# say plainly that the port is open.
if [ "$HOST" = "0.0.0.0" ]; then
    LAN_IP="$(ipconfig getifaddr en0 2>/dev/null || ipconfig getifaddr en1 2>/dev/null || echo 127.0.0.1)"
    CLIENT_HOST="$LAN_IP"
    REACH="all interfaces - reachable from the LAN at $LAN_IP"
    if [ -n "$API_KEY" ]; then
        REACH="$REACH, api-key required"
    else
        REACH="$REACH, NO auth (pass --api-key to require one)"
    fi
else
    CLIENT_HOST="$HOST"
    REACH="$HOST only"
fi

cat <<EOF

  model    $(basename "$MODEL")
  context  $CTX tokens, KV $KV_TYPE   (model default is 262144 - deliberately not used)
  layers   $NGL on GPU
  listen   $REACH
  endpoint http://$CLIENT_HOST:$PORT/v1

  OpenAI-protocol clients:
    base_url  http://$CLIENT_HOST:$PORT/v1
    api_key   ${API_KEY:-any non-empty string}
    model     $ALIAS

    curl http://$CLIENT_HOST:$PORT/v1/chat/completions \\
      -H 'Content-Type: application/json' \\
      -d '{"model":"$ALIAS","messages":[{"role":"user","content":"hello"}]}'

  Thinking is $([ "$THINKING" = "1" ] && echo "ON - answers arrive in message.reasoning_content
  then message.content; pass --no-think for plain content only" || echo "OFF - everything arrives in message.content").

  Expect ~8.0 GB resident at 8K context. Ctrl-C to stop.

EOF

exec "$SERVER_BIN" "${args[@]}"
