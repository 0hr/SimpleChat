# SimpleChat
### CS 550 Advanced OS - Programming Assignment 3
### Harun Pekacar - A20607262

SimpleChat is a Qt 6 desktop client that demonstrates a UDP-based, peer-to-peer messaging with anti-entropy. 
Instances discover neighbours on localhost, exchange vector clocks, and deliver both direct and broadcast chat messages reliably.
It also has Destination-Sequenced Distance-Vector (DSDV) routing and basic NAT via route rumors, and an --noforward rendezvous mode.

## Features

* **UDP peers:** Each node binds to a local port, discover nearby ports, and exchanges `Hello` messages to build a live peer list.
* **Direct & broadcast messaging:** Messages contain `ChatText`, `Origin`, `Destination` (use `-1` for broadcast), and per-origin sequence numbers so peers can order delivery.
* **Reliable delivery:** QUdpSocket transport adds acknowledgements, retry timers, and per peer tracking to resend messages that are not confirmed within ~1.5 seconds.
* **Anti-entropy gossip:** Peers periodically publish vector clocks and push any missing messages.
* **Forwarding:** Private messages include a HopLimit and are forwarded toward the next hop if not local.
* **Routing (DSDV):** Each node keeps a next hop per destination with a sequence number; newer routes win, and direct routes win on ties. Each node sends small periodic reachability updates that spread and refresh routes.
* **Rendezvous (no forward):** A node can run with `--noforward`. It helps discovery but does not relay chat.
* **NAT:** Each rumor carries the last seen public IP and port so peers can attempt direct connections.

## Prerequisites

* C++20 compiler
* CMake
* Qt 6 (Widgets + Network modules)

## Build & Run

The helper script `build_run.sh` builds the project and launches four peers listening on ports 9001–9004:

```bash
chmod +x build_run.sh
./build_run.sh
```

You can also build manually:

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

Launch a peer by providing an ID, a UDP port, and optional seed peers (`host:port` pairs):

```bash
./build/SimpleChat --id 1 --port 9001 --peers 127.0.0.1:9002,127.0.0.1:9003
```

UI:

1. Enter a unique ID, bind host (defaults to `127.0.0.1`), and local port.
2. Supply initial peers (comma-separated `host:port`) or rely on local discovery.
3. Click **Connect**. Use the **Add Peer** controls to attach to new hosts later.
4. Set `To` to a peer ID for direct messages or `-1` to broadcast to the cluster.
5. The Routes table shows the current route per destination

## Routes table

* Dest: destination node id
* Next Hop: next peer on the path
* SeqNo: freshness for routing, higher is newer
* Direct: Yes for neighbor, No for multi-hop
* Endpoint: last seen IP and port
* Updated: time since last update

## NAT traversal (Linux)
Set up namespaces and a bridge:

```bash
sudo bash netns_setup.sh
```
Clean up when done:

```bash
sudo bash netns_remove.sh
```

## Test Scripts

- `test1`: Peer 1 sends broadcast message to all peers.
- `test2`: Route rumors appear in their routes table and Peer 1 sends message to Peer 3 via Peer 2
- `test3`: Peer 2 sends private message to Peer 3
- `test4`: Peer 4 sends private message to Peer 4 via DSDV multi hop
- `test5`: N1 and N1 peers discover enpoints via route rumors and send message using S noforward (run netns_setup.sh before testing)
- `test6`: Peer 1 and Peer 3 discover each other via route rumors and Message Peer 1 to Peer 3 may not deliver through 2.