#!/bin/sh
# Compile and flash Claude Monitor v2 to the CYD with arduino-cli.
#   sh flash.sh                      # auto-picks the first USB serial port
#   sh flash.sh /dev/cu.usbserial-10 # or name the port
# Saved Wi-Fi and token are kept (only the app is rewritten).
set -e
cd "$(dirname "$0")"
# 115200 baud: this board's USB chip drops out at the default 921600
FQBN="esp32:esp32:esp32:PartitionScheme=huge_app,UploadSpeed=115200"
PORT="${1:-$(ls /dev/cu.usbserial-* /dev/cu.wchusbserial* /dev/cu.SLAB_USBtoUART 2>/dev/null | head -1)}"
if [ -z "$PORT" ]; then echo "No USB serial port found. Plug in the monitor or pass the port."; exit 1; fi
echo "Flashing to $PORT"
arduino-cli compile --fqbn "$FQBN" --upload --port "$PORT" .
echo "Done. Watching serial output (Ctrl-C to stop)..."
arduino-cli monitor --port "$PORT" --config baudrate=115200
