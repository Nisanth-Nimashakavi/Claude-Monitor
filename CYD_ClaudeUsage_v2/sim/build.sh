#!/bin/sh
# Builds the Mac preview simulator and renders every screen to sim/out/*.png
set -e
cd "$(dirname "$0")/.."
mkdir -p sim/out
c++ -std=c++17 -O2 -Wall -Wno-unused-function -o sim/out/sim sim/sim.cpp screens.cpp gfx.cpp
./sim/out/sim sim/out
for f in sim/out/*.bmp; do
  sips -s format png "$f" --out "${f%.bmp}.png" >/dev/null && rm "$f"
done
echo "PNG previews in sim/out/"
