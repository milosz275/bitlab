#!/bin/bash
# This script installs the BitLab executable in /usr/local/bin

set -e

INSTALL_DIR="/usr/local/bin"                    # directory to install BitLab executable
EXECUTABLE_NAME="bitlab"                        # command to run BitLab after installation
REPO_URL="https://github.com/milosz275/bitlab"  # BitLab repository URL
BUILD_DIR="/tmp/bitlab"                         # temporary directory to clone and build BitLab

if [ "$EUID" -ne 0 ]; then
    echo "Please run as root or with sudo"
    exit 1
fi

if ! command -v make &> /dev/null; then
    echo "make is required to run this script. Please install make first."
    echo "sudo apt install make"
    exit 1
fi

echo "Cloning $EXECUTABLE_NAME repository..."
git clone $REPO_URL $BUILD_DIR

echo "Building $EXECUTABLE_NAME..."
cd $BUILD_DIR
make

echo "Installing $EXECUTABLE_NAME..."
cp $BUILD_DIR/bitlab/build/bin/main $INSTALL_DIR/$EXECUTABLE_NAME
chmod +x $INSTALL_DIR/$EXECUTABLE_NAME

echo "$EXECUTABLE_NAME has been installed in $INSTALL_DIR"
echo "Cleaning up..."
rm -rf $BUILD_DIR

echo "Installation complete. Please run '$EXECUTABLE_NAME' to complete the setup."
