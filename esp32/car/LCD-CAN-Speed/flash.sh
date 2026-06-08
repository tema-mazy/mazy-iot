#!/bin/bash
set -e

# If IDF_PATH is not set or export.sh is missing, resolve the correct directory
if [ -z "$IDF_PATH" ] || [ ! -f "$IDF_PATH/export.sh" ]; then
    echo "Error: Could not locate ESP-IDF export.sh. Please set IDF_PATH."
    exit 1
fi

echo "Using IDF_PATH: $IDF_PATH"

# Source ESP-IDF environment
. "$IDF_PATH/export.sh"

# Default to /dev/ttyUSB0 if no port is specified as an argument
PORT="${1:-/dev/cu.usbmodem1101}"

echo "Flashing firmware to ESP32-C6 on port: $PORT"
idf.py -p "$PORT" flash
idf.py -p "$PORT" monitor
