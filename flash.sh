#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

PORT="${1:-/dev/ttyACM0}"
SKETCH="firmware/epaper42_web"
SECRETS="$SKETCH/secrets.h"

if [ -n "${ARDUINO_CLI:-}" ]; then
  CLI="$ARDUINO_CLI"
elif command -v arduino-cli >/dev/null 2>&1; then
  CLI="arduino-cli"
elif [ -x ../tools/arduino-cli ]; then
  CLI="../tools/arduino-cli"          # local development workspace
elif [ -x ./tools/arduino-cli ]; then
  CLI="./tools/arduino-cli"
else
  echo "arduino-cli not found. Install it or set ARDUINO_CLI=/path/to/arduino-cli"
  exit 1
fi

if [ ! -f "$SECRETS" ]; then
  echo "note: no $SECRETS"
  echo "      building with placeholder Wi-Fi; after flashing use the serial console:"
  echo "      wifi set   (SSID + hidden password; saved to NVS)"
fi

echo "== compile =="
"$CLI" compile --fqbn esp32:esp32:esp32 "$SKETCH"
echo "== upload to $PORT =="
"$CLI" upload -p "$PORT" --fqbn esp32:esp32:esp32 "$SKETCH"
echo "== done =="
echo "Open the serial monitor at 115200 to see the IP address, then type: wifi set"
