#!/usr/bin/env bash
# Open the serial console in a way that does not disturb the ESP32 auto-reset
# circuit more than necessary, and leave the modem lines deasserted on exit.
#
#   ./tools/serial.sh [port] [baud]
#
# Why this exists: the CH343P on the Waveshare board drives EN/IO0 through the
# usual two-transistor auto-reset circuit, so whenever a terminal program
# toggles DTR/RTS (typically on open *and* on close) the ESP32 can be reset.
# `-hupcl` stops the kernel from dropping the lines when the port is closed,
# which avoids the reset pulse in most cases.
set -u
PORT="${1:-/dev/ttyACM0}"
BAUD="${2:-115200}"

if [ ! -e "$PORT" ]; then
  echo "no such port: $PORT" >&2
  exit 1
fi

# Best effort: nobody should be logged in as root just to talk to a PDA.
if [ ! -w "$PORT" ]; then
  echo "note: $PORT is not writable; run: sudo chmod 666 $PORT" >&2
fi

stty -F "$PORT" "$BAUD" cs8 -cstopb -parenb -crtscts -hupcl clocal raw -echo 2>/dev/null || true

if command -v minicom >/dev/null 2>&1; then
  exec minicom -D "$PORT" -b "$BAUD" -8 --color=no
elif command -v picocom >/dev/null 2>&1; then
  exec picocom -b "$BAUD" --imap lfcrlf "$PORT"
else
  echo "install minicom or picocom, or use: screen $PORT $BAUD" >&2
  exit 1
fi
