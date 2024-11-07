# BitLab: A Bitcoin P2P Network and Blockchain Explorer

[![Make](https://github.com/milosz275/bitlab/actions/workflows/makefile.yml/badge.svg)](https://github.com/milosz275/bitlab/actions/workflows/makefile.yml)
[![CodeQL](https://github.com/milosz275/bitlab/actions/workflows/codeql.yml/badge.svg)](https://github.com/milosz275/bitlab/actions/workflows/codeql.yml)
[![Doxygen Pages](https://github.com/milosz275/bitlab/actions/workflows/doxygen-pages.yml/badge.svg)](https://github.com/milosz275/bitlab/actions/workflows/doxygen-pages.yml)
[![License](https://img.shields.io/github/license/milosz275/bitlab)](/LICENSE)

![Logo](assets/logo.png)

BitLab is an interactive command-line tool for exploring and interacting with the Bitcoin P2P network and blockchain. Inspired by SockLab, BitLab provides a user-friendly interface for learning and working with Bitcoin's peer-to-peer networking protocols without needing to write low-level C code. The tool allows users to perform a wide range of operations on the Bitcoin network, such as peer discovery, connecting to peers, sending messages, and retrieving blockchain data.

BitLab acts as a powerful educational resource and debugging tool for understanding how Bitcoin nodes communicate, synchronize blocks, and process transactions by interacting with the Bitcoin network in real-time. The tool is designed to be user-friendly, intuitive, and informative, providing users with a comprehensive view of the Bitcoin network's inner workings using the Bitcoin protocol.

- [GitHub Repo](https://github.com/milosz275/bitlab)
- [Doxygen Docs](https://milosz275.github.io/bitlab)

> [!NOTE]
> This project is a work in progress.

## Table of Contents

- [Installation](#installation)
- [Uninstallation](#uninstallation)
- [Usage](#usage)
- [Config directory](#config-directory)
- [Features](#features)
- [Acknowledgements](#acknowledgements)
- [License](#license)

## Installation

> [!NOTE]
> Default installation directory is `/usr/local/bin`

To install [BitLab](https://github.com/milosz275/bitlab), simply run the following command in your terminal:

```bash
curl -s https://raw.githubusercontent.com/milosz275/bitlab/main/install.sh | sudo bash
```

## Uninstallation

To uninstall [BitLab](https://github.com/milosz275/bitlab), simply run the following command in your terminal:

```bash
curl -s https://raw.githubusercontent.com/milosz275/bitlab/main/uninstall.sh | sudo bash -s -- -y
```

## Building from Source

To build BitLab from source, follow these steps:

1. Install the required dependencies:

    ```bash
    sudo apt-get update
    sudo apt-get install -y cmake libreadline-dev libssl-dev
    ```

2. Clone the repository:

    ```bash
    git clone https://github.com/milosz275/bitlab.git
    ```

3. Change to the project directory:

    ```bash
    cd bitlab
    ```

4. Build the project:

    ```bash
    make
    ```

5. Use the `main` executable:

    ```bash
    bitlab/build/bin/main
    ```

## Usage

<!-- [ ] Add usage instructions -->

## Config directory

The default configuration directory is `~/.bitlab`. The configuration directory contains the following files:

- `logs/bitlab.log`: BitLab main log file
- `history/cli_history.txt`: BitLab CLI command history file
<!-- - `bitlab.conf`: BitLab configuration file
- `peers.dat`: Peer list file
- `blocks.dat`: Block list file
- `txs.dat`: Transaction list file -->

## Features

### 1. Peer Discovery

Easily find and connect with peers using various methods:

- **Discovery Options:**
  - Via Command Line Parameters
  - Predefined Seeds
  - DNS Lookup

- **Bootstrapping with Peer Data:**
  - Use the `getaddr` message to request an `addr` message containing active peers.
  - `addr` messages will provide a list of IP addresses and ports.

- **Peer Information:**
  - Print out IP addresses of discovered peers.
  - Optionally, perform recursive discovery to find more peers.

### 2. Connection to Peers

Seamlessly establish and maintain connections with peers:

- **Initial Connection:**
  - Exchange `version` messages to introduce versions.
  - Follow up with `verack` messages to confirm the connection is successful.

### 3. Peer Maintenance

Stay connected with active peers and monitor peer availability:

- **Peer Availability Checks:**
  - Use `ping` messages to check peer responsiveness and connection health.

- **Network Alerts:**
  - Send `alert` messages to notify peers of important network events.

### 4. Error Handling & Diagnostics

Handle errors gracefully and communicate network events transparently:

- **Error Reporting:**
  - Use `reject` messages to report any client-side errors.

- **Diagnostics:**
  - Use `message` messages to log diagnostics and troubleshooting information.

### 5. Block Inventory, Exchange, and Transactions

Efficiently share and request blocks and transactions with peers:

- **Inventory Announcements:**
  - Use `inv` messages to announce available blocks or transactions.

- **Requesting Data:**
  - `getdata` message: Request specific blocks or transactions by their hash.
  - `getblocks` message: Request an inventory list for blocks within a specified range.
  - `getheaders` message: Request headers of blocks in a specific range for easy synchronization.

- **Transaction and Block Sharing:**
  - `tx` message: Announce new transactions.
  - `block` message: Send or advertise a specific block.
  - `headers` message: Share up to 2,000 block headers for faster synchronization.

## Acknowledgements

- [Thread-safe logging](https://github.com/milosz275/secure-chat/blob/main/common/include/log.h)

## License

This project is licensed under the MIT License - see the [LICENSE](https://github.com/milosz275/bitlab/blob/main/LICENSE) file for details.
