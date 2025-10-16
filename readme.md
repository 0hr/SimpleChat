# SimpleChat
### CS 550 Advanced OS - Programming Assignment
### Harun Pekacar - A20607262

SimpleChat is a Qt 6 desktop client that demonstrates a UDP-based, peer-to-peer messaging with anti-entropy. Instances discover neighbours on localhost, exchange vector clocks, and deliver both direct and broadcast chat messages reliably.

## Features

* **UDP peers:** Each node binds to a local port, discover nearby ports, and exchanges `Hello` messages to build a live peer list.
* **Direct & broadcast messaging:** Messages contain `ChatText`, `Origin`, `Destination` (use `-1` for broadcast), and per-origin sequence numbers so peers can order delivery.
* **Reliable delivery:** QUdpSocket transport adds acknowledgements, retry timers, and per peer tracking to resend messages that are not confirmed within ~1.5 seconds.
* **Anti-entropy gossip:** Peers periodically publish vector clocks and push any missing messages.

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

Within the UI:

1. Enter a unique ID, bind host (defaults to `127.0.0.1`), and local port.
2. Supply initial peers (comma-separated `host:port`) or rely on local discovery.
3. Click **Connect**. Use the **Add Peer** controls to attach to new hosts later.
4. Set `To` to a peer ID for direct messages or `-1` to broadcast to the cluster.

## Test Scripts

Scripts `build_run_test1.sh` – `build_run_test6.sh` exercise the UDP mesh behaviours:

- `test1`: peer 2 sends a single direct message to peer 3.
- `test2`: peer 1 broadcasts to all peers (`-1` destination).
- `test3`: peer 1 broadcasts while peer 3 is offline; peer 3 and peer 4 joins later and receives the messages via anti-entropy.
- `test4`: peer 1 pushes a quick messages of ordered messages to peer 2.
- `test5`: peers 1/2/4 send message to peer 3.
- `test6`: peer 1 broadcasts a multi-line payload to confirm framing.
- `test7`: auto discovery, peers find each other locally.