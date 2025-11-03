#!/usr/bin/env bash
set -euo pipefail

require_root() {
  if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
    echo "This script requires root (sudo)." >&2
    exit 1
  fi
}

require_root

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

RV_ADDR=0.0.0.0
RV_HOST_IP=10.200.0.1

"$bin" --id S --port 45678 --bind "$RV_ADDR" --noforward & pids+=($!)

sleep 1

ip netns exec nat1 "$bin" --id N1 --port 11111 --bind 0.0.0.0 --peers ${RV_HOST_IP}:45678 --test_wait_time 6000 --test_message "Hello N2 across rendezvous" --test_peer N2 --test_count 1 & pids+=($!)

ip netns exec nat2 "$bin" --id N2 --port 22222 --bind 0.0.0.0 --peers ${RV_HOST_IP}:45678 & pids+=($!)

echo "Expect: N1 and N2 discover endpoints via route rumors; direct PM N1 -> N2 succeeds."

wait || true

