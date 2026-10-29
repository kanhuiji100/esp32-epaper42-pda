#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

PORT="${1:-/dev/ttyACM0}"
CLI="${ARDUINO_CLI:-arduino-cli}"
SKETCH="firmware/epaper42_web"
SECRETS="$SKETCH/secrets.h"

if [ ! -f "$SECRETS" ]; then
  echo "Missing $SECRETS"
  echo "  cp $SKETCH/secrets.example.h $SECRETS"
  echo "  # then edit WIFI_SSID / WIFI_PASSWORD"
  exit 1
fi

if ! command -v "$CLI" >/dev/null 2>&1 && [ ! -x "$CLI" ]; then
  echo "arduino-cli not found. Install it or set ARDUINO_CLI=/path/to/arduino-cli"
  exit 1
fi

echo "== compile =="
"$CLI" compile --fqbn esp32:esp32:esp32 "$SKETCH"
echo "== upload to $PORT =="
"$CLI" upload -p "$PORT" --fqbn esp32:esp32:esp32 "$SKETCH"
echo "== done =="
echo "Open the serial monitor at 115200 to see the IP address."
