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

"$bin" --id 1 --port 9101 & pids+=($!)
"$bin" --id 2 --port 9102 --peers 127.0.0.1:9101 & pids+=($!)
sleep 3
"$bin" --id 3 --port 9103 & pids+=($!)
"$bin" --id 4 --port 9104 --peers 127.0.0.1:9103 & pids+=($!)

echo "Launched 4 SimpleChat instances for autodiscovery (PIDs: ${pids[*]})."
wait || true
