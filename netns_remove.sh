#!/usr/bin/env bash
set -euo pipefail

require_root() {
  if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
    echo "This script requires root (sudo)." >&2
    exit 1
  fi
}

require_root

cleanup_veth() {
  local ifname=$1
  if ip link show "$ifname" >/dev/null 2>&1; then
    ip link del "$ifname" 2>/dev/null || true
  fi
}

for ns in nat1 nat2; do
  if ip netns list | grep -q "^$ns\b"; then
    echo "Deleting namespace $ns..."
    ip netns exec "$ns" ip link show veth-${ns} >/dev/null 2>&1 && \
      ip netns exec "$ns" ip link del veth-${ns} 2>/dev/null || true
    ip netns delete "$ns"
  fi
done

cleanup_veth veth-nat1-br || true
cleanup_veth veth-nat2-br || true

if ip link show br-simplechat >/dev/null 2>&1; then
  echo "Removing bridge ..."
  ip link set br-simplechat down || true
  ip link del br-simplechat || true
fi

echo "netns remove complete."

