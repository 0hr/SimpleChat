#!/usr/bin/env bash
set -euo pipefail

current_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$current_dir/" && pwd)"
build="$root/build"
bin="$build/SimpleChat"

# Build if needed
if [[ ! -x "$bin" ]]; then
  echo "Building project..."
  mkdir -p "$build"
  (cd "$build" && cmake .. >/dev/null && cmake --build . -j)
fi

pids=()

cleanup() {
  trap - INT TERM EXIT
  echo -e "\nShutting down..."
  for pid in "${pids[@]:-}"; do
    if kill -0 "$pid" 2>/dev/null; then
      kill -9 "$pid" 2>/dev/null || true
    fi
  done
  wait || true
}

trap cleanup INT TERM EXIT

"$bin" --id 1 --port 9001 --peers 127.0.0.1:9002,127.0.0.1:9003,127.0.0.1:9004 & pids+=($!)
"$bin" --id 2 --port 9002 --peers 127.0.0.1:9001,127.0.0.1:9003,127.0.0.1:9004 --test_wait_time 1500 --test_message "Hi 3" --test_peer 3 --test_count 1 & pids+=($!)
"$bin" --id 3 --port 9003 --peers 127.0.0.1:9001,127.0.0.1:9002,127.0.0.1:9004 & pids+=($!)
"$bin" --id 4 --port 9004 --peers 127.0.0.1:9001,127.0.0.1:9002,127.0.0.1:9003 & pids+=($!)

echo "Expect peer 3 to receive exactly one direct PM from peer 2."

wait || true

