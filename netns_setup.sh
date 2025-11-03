#!/usr/bin/env bash
set -euo pipefail

require_root() {
  if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
    echo "This script requires root (sudo)." >&2
    exit 1
  fi
}

cmd_exists() { command -v "$1" >/dev/null 2>&1; }

require_root

if ! cmd_exists ip; then
  echo "ip(8) command not found. Install iproute2." >&2
  exit 1
fi

set +e
ip link show br-simplechat >/dev/null 2>&1
has_bridge=$?
set -e

if [[ $has_bridge -ne 0 ]]; then
  echo "Creating bridge br-simplechat..."
  ip link add name br-simplechat type bridge
  ip addr add 10.200.0.1/24 dev br-simplechat || true
  ip link set br-simplechat up
else
  echo "Bridge br-simplechat already exists."
fi

for ns in nat1 nat2; do
  if ! ip netns list | grep -q "^$ns\b"; then
    echo "Creating namespace $ns..."
    ip netns add "$ns"
  else
    echo "Namespace $ns already exists."
  fi
done

setup_ns() {
  local ns=$1
  local veth_ns=$2
  local veth_br=$3
  local ipaddr=$4

  if ! ip link show "$veth_br" >/dev/null 2>&1; then
    echo "Creating veth pair $veth_ns <-> $veth_br..."
    ip link add "$veth_ns" type veth peer name "$veth_br"
  fi

  ip link set "$veth_ns" netns "$ns" 2>/dev/null || true

  ip link set "$veth_br" master br-simplechat 2>/dev/null || true
  ip link set "$veth_br" up

  # Configure namespace side
  ip netns exec "$ns" ip link set lo up
  ip netns exec "$ns" ip link set "$veth_ns" up
  ip netns exec "$ns" ip addr add "$ipaddr" dev "$veth_ns" 2>/dev/null || true
  ip netns exec "$ns" ip route add default via 10.200.0.1 dev "$veth_ns" 2>/dev/null || true
}

setup_ns nat1 veth-nat1 veth-nat1-br 10.200.0.2/24
setup_ns nat2 veth-nat2 veth-nat2-br 10.200.0.3/24

echo "netns setup complete: nat1(10.200.0.2), nat2(10.200.0.3), bridge br-simplechat(10.200.0.1)."

