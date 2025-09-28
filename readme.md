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

A helper script `build_run.sh` is provided to build and run the application.

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