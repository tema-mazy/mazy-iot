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

# Run build
idf.py build
