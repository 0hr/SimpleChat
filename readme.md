# SimpleChat
### CS 550 Advanced OS - Programming Assignment - Part 1
### Harun Pekacar - A20607262

A simple p2p chat application built with C++ and Qt that uses a ring network for communication.

## Features

*   **P2P communication:** Chat with other users directly without a central server.
*   **Ring network:** Each chat instance connects to the next one in a ring, and messages are forwarded until they reach their destination.
*   **Simple UI:** A basic graphical user interface to send and receive messages.
*   **Command-line arguments:** Configure the user ID, port, and the next peer's port and peer list via the command line.

## Prerequisites

*   C++ compiler
*   CMake
*   Qt 6

## How to Build and Run

A helper script `build_run.sh` is provided to build and run 4 nodes.

1.  **Make the script executable:**
    ```bash
    chmod +x build_run.sh
    ```

2.  **Run the script:**
    ```bash
    ./build_run.sh
    ```

This script will:
1.  Create a `build` directory.
2.  Run `cmake` to generate the build files.
3.  Build the project.
4.  Run 4 instances of the application, connected in a ring, with IDs 1, 2, 3, and 4, on ports 9001, 9002, 9003, and 9004.

You can also build and run the application manually.

### Building

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### Running

To run an instance of the application, you need to specify the user ID, the port, and the port of the next peer in the ring and other peers.

```bash
./build/SimpleChat --id <id> --port <port> --next <peer-port> --peers <comma-separated-list-of-other-peer-ids>
```

For example, to start a chat client with ID "1" on port 9001, which connects to a peer on port 9002, you would run:

```bash
./build/SimpleChat --id 1 --port 9001 --next 9002 --peers 2,3,4
```

To create a ring of 4 peers, you would run the following commands in separate terminals:

```bash
./build/SimpleChat --id 1 --port 9001 --next 9002 --peers 2,3,4
./build/SimpleChat --id 2 --port 9002 --next 9003 --peers 1,3,4
./build/SimpleChat --id 3 --port 9003 --next 9004 --peers 1,2,4
./build/SimpleChat --id 4 --port 9004 --next 9001 --peers 1,2,3
```

### Testing 

#### Test 1
A helper scripts `build_run_test1.sh` is provided to build and run 4 nodes.

**Test Case:** Peer 2 sends "Hi 3 (single hop)" to Peer 3

**Expect:** Message Shows on Peer 3 only. Path 2 to 3

#### Test 2
A helper scripts `build_run_test2.sh` is provided to build and run 4 nodes.

**Test Case:** Peer 1 sends "Hi 3 (two hops)" to Peer 3

**Expect:** Message Shows on Peer 3 only. Path 1 to 2 to 3

#### Test 3
A helper scripts `build_run_test3.sh` is provided to build and run 4 nodes.

**Test Case:** Peer 3 sends "Hi 2 (travel to end, 1 to 2)" to Peer 2

**Expect:** Message Shows on Peer 3 only. Path 3 to 4 to 1 to 2

#### Test 4
A helper scripts `build_run_test4.sh` is provided to build and run 4 nodes.

**Test case:**  Peer 1 sends messages to Peer 3 quickly Message, Message, Message

**Expect:** On Peer 3, messages appear Message, Message, Message in order from Peer 1.

#### Test 5
A helper scripts `build_run_test5.sh` is provided to build and run 4 nodes.

**Test case:**  
- Peer 1 sends messages to Peer 3 quickly Message FROM Peer 1, Message FROM Peer 1, Message FROM Peer 1
- Peer 2 sends messages to Peer 3 quickly Message FROM Peer 2, Message FROM Peer 2, Message FROM Peer 2
- Peer 4 sends messages to Peer 3 quickly Message FROM Peer 4, Message FROM Peer 4, Message FROM Peer 4

**Expect:** 

On Peer 3, messages appear in order

- Message FROM Peer 1, Message FROM Peer 1, Message FROM Peer 1
- Message FROM Peer 2, Message FROM Peer 2, Message FROM Peer 2
- Message FROM Peer 4, Message FROM Peer 4, Message FROM Peer 4


#### Test 6
A helper scripts `build_run_test6.sh` is provided to build and run 4 nodes.

**Test case:**  Peer 1 sends messages to Peer 4 quickly Message \n test (line wrap)

**Expect:** On Peer 4, messages appear Message \n test (line wrap) 