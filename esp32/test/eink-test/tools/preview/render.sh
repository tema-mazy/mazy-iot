#!/bin/sh
# Render the dashboard screens to PNG without touching hardware.
# Usage: tools/preview/render.sh   (run from the project root)
set -e
cd "$(dirname "$0")"
mkdir -p out
cc -std=gnu11 -Wall -O1 \
   -I../../main -Istub \
   -DCONFIG_DASH_WEATHER_PLACE='"Krakow, Borek"' \
   preview.c ../../main/screens.c ../../main/epaper_gfx.c ../../main/fonts.c \
   -o preview
./preview
for f in out/*.pgm; do
    sips -s format png "$f" --out "${f%.pgm}.png" >/dev/null
    rm "$f"
done
echo "PNGs in tools/preview/out/"
