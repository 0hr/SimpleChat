#!/usr/bin/env bash
set -euo pipefail

current_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$current_dir/" && pwd)"
build="$root/build"
bin="$build/SimpleChat"

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

"$bin" --id 1 --port 9001 --peers 127.0.0.1:9002 --test_wait_time 2500 --test_message "PM to 3 via rendezvous" --test_peer 3 --test_count 1 & pids+=($!)
"$bin" --id 2 --port 9002 --noforward & pids+=($!)
"$bin" --id 3 --port 9003 --peers 127.0.0.1:9002 & pids+=($!)

echo "Expect: Peer 1 and Peer 3 discover each other via route rumors and Message Peer 1 to Peer 3 may not deliver through 2."


wait || true

