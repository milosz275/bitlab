#!/bin/bash
# This script uninstalls the BitLab executable from /usr/local/bin

INSTALL_DIR="/usr/local/bin"                    # directory to uninstall BitLab executable
UTILITY_NAME="bitlab"                           # the same as EXECUTABLE_NAME in install.sh

if [ "$EUID" -ne 0 ]; then
    echo "Please run as root or with sudo"
    exit 1
fi

if [[ "$1" != "-y" ]]; then
    echo "Are you sure you want to uninstall $UTILITY_NAME? [y/n]"
    read -r confirm
    if [ "$confirm" != "y" ]; then
        echo "Uninstallation aborted."
        exit 0
    fi
fi

if [ -f "$INSTALL_DIR/$UTILITY_NAME" ]; then
    echo "Removing $INSTALL_DIR/$UTILITY_NAME..."
    sudo rm -f "$INSTALL_DIR/$UTILITY_NAME"
else
    echo "$UTILITY_NAME not found in $INSTALL_DIR."
    exit 1
fi

echo "$UTILITY_NAME has been uninstalled successfully."
echo "Note: The configuration files are not removed. You may want to remove them manually from ~/.bitlab"
exit 0
