# BitLab: A Bitcoin P2P Network and Blockchain Explorer

[![Make](https://github.com/milosz275/bitlab/actions/workflows/makefile.yml/badge.svg)](https://github.com/milosz275/bitlab/actions/workflows/makefile.yml)
[![CodeQL](https://github.com/milosz275/bitlab/actions/workflows/codeql.yml/badge.svg)](https://github.com/milosz275/bitlab/actions/workflows/codeql.yml)
[![Doxygen Pages](https://github.com/milosz275/bitlab/actions/workflows/doxygen-pages.yml/badge.svg)](https://github.com/milosz275/bitlab/actions/workflows/doxygen-pages.yml)
[![License](https://img.shields.io/github/license/milosz275/bitlab)](LICENSE)

![Logo](assets/logo.png)

BitLab is an interactive command-line tool for exploring and interacting with the Bitcoin P2P network and blockchain. Inspired by SockLab, BitLab provides a user-friendly interface for learning and working with Bitcoin's peer-to-peer networking protocols without needing to write low-level C code. The tool allows users to perform a wide range of operations on the Bitcoin network, such as peer discovery, connecting to peers, sending messages, and retrieving blockchain data.

BitLab acts as a powerful educational resource and debugging tool for understanding how Bitcoin nodes communicate, synchronize blocks, and process transactions.

- [GitHub Repo](https://github.com/milosz275/bitlab)
- [Doxygen Docs](https://milosz275.github.io/bitlab)

> [!NOTE]
> This project is a work in progress.

## Table of Contents

- [Features](#features)
- [Installation](#installation)
- [Uninstallation](#uninstallation)
- [Usage](#usage)
- [Config directory](#config-directory)
- [Acknowledgements](#acknowledgements)
- [License](#license)

## Features

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

## Usage

## Config directory

The default configuration directory is `~/.bitlab`. The configuration directory contains the following files:

- `logs/bitlab.log`: BitLab main log file
- `history/cli_history.txt`: BitLab CLI command history file
<!-- - `bitlab.conf`: BitLab configuration file
- `peers.dat`: Peer list file
- `blocks.dat`: Block list file
- `txs.dat`: Transaction list file -->

## Acknowledgements

- [Thread-safe logging](https://github.com/milosz275/secure-chat/blob/main/common/include/log.h)

## License

This project is licensed under the MIT License - see the [LICENSE](https://github.com/milosz275/bitlab/blob/main/LICENSE) file for details.
