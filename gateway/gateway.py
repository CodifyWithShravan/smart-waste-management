"""
============================================================
 Smart Waste Management — Raspberry Pi LoRa Gateway
============================================================

 Hardware Setup:
   Raspberry Pi ↔ REYAX RYLR998 LoRa Module
     - Connect via USB-to-Serial adapter or GPIO UART
     - Default port: /dev/ttyUSB0 (USB adapter) or /dev/serial0 (GPIO)

 Data Flow:
   LoRa RX → Serial → This Script → HTTP POST → Node.js Backend

 The ESP32 sends CSV payloads via LoRa:
   BIN-001,75,17.4200,78.6560

 The RYLR998 receives them as:
   +RCV=<addr>,<length>,<payload>,<RSSI>,<SNR>
   Example: +RCV=0,25,BIN-001,75,17.4200,78.6560,-45,12

 This script parses the payload and POSTs to the backend API.

 Usage:
   pip install pyserial requests
   python gateway.py
============================================================
"""

import serial
import serial.tools.list_ports
import requests
import time
import sys
import os
from datetime import datetime
from collections import OrderedDict

# ============================================================
#  CONFIGURATION — Change these to match your setup
# ============================================================

# Serial port for the LoRa module
# Auto-detect: finds the ESP32 USB serial port automatically
# Override: set to a specific port like "/dev/ttyUSB0" or "/dev/cu.usbserial-0001"
SERIAL_PORT = None  # None = auto-detect
SERIAL_BAUD = 115200

# Backend API URL
# When running backend on the SAME machine, use 127.0.0.1
# When running backend on a separate server, use that server's IP
BACKEND_URL = "http://127.0.0.1:5050/api/bins/update"

# How many times to retry a failed HTTP POST
HTTP_RETRIES = 3
HTTP_RETRY_DELAY = 2  # seconds

# Expected bins (for the status tracker)
EXPECTED_BINS = ["BIN-001", "BIN-002", "BIN-003"]

# ============================================================
#  MULTI-BIN STATUS TRACKER
# ============================================================

# Tracks the last-known state of each bin
bin_status = OrderedDict()


def update_bin_status(data):
    """Updates the in-memory status tracker for a bin."""
    bin_id = data["binId"]
    bin_status[bin_id] = {
        "fillLevel": data["fillLevel"],
        "latitude":  data.get("latitude", 0),
        "longitude": data.get("longitude", 0),
        "rssi":      data.get("rssi", "N/A"),
        "snr":       data.get("snr", "N/A"),
        "lastSeen":  datetime.now(),
    }


def print_status_dashboard():
    """Prints a compact summary table of all known bins."""
    now = datetime.now()
    print("\n" + "=" * 62)
    print("  📊 MULTI-BIN STATUS DASHBOARD")
    print("=" * 62)
    print(f"  {'Bin ID':<10} {'Fill':<8} {'Signal':<12} {'Last Seen':<18} {'Status'}")
    print("  " + "-" * 58)

    for bin_id in EXPECTED_BINS:
        if bin_id in bin_status:
            info = bin_status[bin_id]
            fill = f"{info['fillLevel']}%"
            rssi = f"{info['rssi']}dBm" if info['rssi'] != "N/A" else "N/A"
            ago = (now - info["lastSeen"]).total_seconds()

            if ago < 60:
                last_seen = f"{int(ago)}s ago"
            elif ago < 3600:
                last_seen = f"{int(ago/60)}m ago"
            else:
                last_seen = f"{int(ago/3600)}h ago"

            # Color-coded status
            if info["fillLevel"] >= 80:
                status = "🔴 FULL"
            elif info["fillLevel"] >= 50:
                status = "🟡 HALF"
            else:
                status = "🟢 OK"

            if ago > 120:
                status = "⚪ STALE"

            print(f"  {bin_id:<10} {fill:<8} {rssi:<12} {last_seen:<18} {status}")
        else:
            print(f"  {bin_id:<10} {'---':<8} {'---':<12} {'never':<18} ⏳ WAITING")

    print("=" * 62 + "\n")

# ============================================================
#  HELPER FUNCTIONS
# ============================================================

def log(message):
    """Timestamped console log."""
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    print(f"[{timestamp}] {message}")


def parse_lora_message(raw_line):
    """
    Parses a +RCV message from the RYLR998 LoRa module.

    Input format:
      +RCV=<addr>,<length>,<payload>,<RSSI>,<SNR>

    The payload itself is CSV:
      BIN-001,75,17.4200,78.6560

    But since payload contains commas, the full line looks like:
      +RCV=0,25,BIN-001,75,17.4200,78.6560,-45,12

    We know the payload has exactly 4 fields (binId, fillLevel, lat, lng),
    so we parse from the known structure:
      parts[0] = addr
      parts[1] = length
      parts[2] = binId
      parts[3] = fillLevel
      parts[4] = latitude
      parts[5] = longitude
      parts[6] = RSSI
      parts[7] = SNR
    """
    try:
        # Remove the "+RCV=" prefix
        if not raw_line.startswith("+RCV="):
            return None

        data = raw_line[5:]  # Strip "+RCV="
        parts = data.split(",")

        if len(parts) < 6:
            log(f"⚠️  Incomplete LoRa message: {raw_line}")
            return None

        result = {
            "binId":     parts[2].strip(),
            "fillLevel": int(parts[3].strip()),
            "latitude":  float(parts[4].strip()),
            "longitude": float(parts[5].strip()),
        }

        # Extract signal info if available
        if len(parts) >= 8:
            result["rssi"] = int(parts[6].strip())
            result["snr"]  = int(parts[7].strip())

        return result

    except (ValueError, IndexError) as e:
        log(f"❌ Parse error: {e} | Raw: {raw_line}")
        return None


def send_to_backend(payload):
    """
    POSTs the parsed payload to the Node.js backend.
    Retries on failure.
    """
    for attempt in range(1, HTTP_RETRIES + 1):
        try:
            response = requests.post(
                BACKEND_URL,
                json=payload,
                timeout=10
            )

            if response.status_code == 200:
                log(f"✅ Sent {payload['binId']} → {payload['fillLevel']}% full")
                return True
            else:
                log(f"⚠️  Backend returned {response.status_code}: {response.text}")

        except requests.ConnectionError:
            log(f"❌ Cannot connect to backend (attempt {attempt}/{HTTP_RETRIES})")
        except requests.Timeout:
            log(f"❌ Backend timeout (attempt {attempt}/{HTTP_RETRIES})")
        except Exception as e:
            log(f"❌ HTTP error: {e} (attempt {attempt}/{HTTP_RETRIES})")

        if attempt < HTTP_RETRIES:
            time.sleep(HTTP_RETRY_DELAY)

    log(f"❌ Failed to send {payload['binId']} after {HTTP_RETRIES} attempts")
    return False


def find_serial_port():
    """
    Auto-detects the ESP32 USB serial port.
    Works on Mac (/dev/cu.usbserial-*) and Linux (/dev/ttyUSB*).
    """
    log("🔍 Auto-detecting ESP32 serial port...")

    # Scan all available serial ports
    ports = serial.tools.list_ports.comports()
    for port in ports:
        desc = (port.description or "").lower()
        hwid = (port.hwid or "").lower()
        # Common ESP32 USB-Serial chips: CP210x, CH340, CH9102, FTDI
        if any(chip in desc for chip in ["cp210", "ch340", "ch910", "ftdi", "usb-serial", "usbserial"]):
            log(f"   Found: {port.device} ({port.description})")
            return port.device
        if any(chip in hwid for chip in ["cp210", "ch340", "ch910", "ftdi"]):
            log(f"   Found: {port.device} ({port.description})")
            return port.device

    # Fallback: try common port names
    import glob
    fallback_patterns = [
        "/dev/cu.usbserial-*",    # Mac
        "/dev/ttyUSB*",            # Linux
        "/dev/ttyACM*",            # Linux (some ESP32s)
    ]
    for pattern in fallback_patterns:
        matches = glob.glob(pattern)
        if matches:
            log(f"   Found: {matches[0]}")
            return matches[0]

    return None


def main():
    log("=========================================")
    log("  Smart Waste Management — LoRa Gateway")
    log(f"  Backend URL: {BACKEND_URL}")
    log("=========================================")

    # Determine the serial port
    port = SERIAL_PORT
    if port is None:
        port = find_serial_port()
        if port is None:
            log("❌ No ESP32 serial port found!")
            log("   → Is the receiver ESP32 plugged in via USB?")
            log("   → Try setting SERIAL_PORT manually in this script.")
            sys.exit(1)

    log(f"  Serial Port: {port}")

    # Connect to the serial port
    while True:
        try:
            ser = serial.Serial(
                port=port,
                baudrate=SERIAL_BAUD,
                timeout=1
            )
            log(f"✅ Connected to {port}")
            break
        except serial.SerialException as e:
            log(f"❌ Serial error: {e}")
            log(f"   Retrying in 5 seconds...")
            time.sleep(5)

    # Flush any buffered data from the ESP32 boot messages
    time.sleep(2)
    ser.reset_input_buffer()
    log("Listening for LoRa messages...\n")

    try:
        serial_buffer = ""

        while True:
            # Read available bytes from the serial port
            raw = ser.readline().decode("utf-8", errors="ignore").strip()

            if not raw:
                continue  # Empty line, keep listening

            # The receiver ESP32 may concatenate multiple +RCV messages
            # Split them into individual messages
            if "+RCV=" in raw:
                # Split on "+RCV=" and process each message
                parts = raw.split("+RCV=")
                for part in parts:
                    part = part.strip()
                    if not part:
                        continue

                    message = "+RCV=" + part
                    log(f"📡 Received: {message}")

                    # Parse the LoRa message
                    data = parse_lora_message(message)

                    if data:
                        # Update the multi-bin status tracker
                        update_bin_status(data)

                        # Log signal quality if available
                        if "rssi" in data:
                            log(f"   Signal: RSSI={data['rssi']}dBm, SNR={data['snr']}dB")

                        # Prepare the POST payload
                        post_payload = {
                            "binId":     data["binId"],
                            "fillLevel": data["fillLevel"],
                            "latitude":  data["latitude"],
                            "longitude": data["longitude"],
                        }

                        # Send to backend
                        send_to_backend(post_payload)

                        # Print the multi-bin status dashboard
                        print_status_dashboard()
                    else:
                        log("⚠️  Could not parse message, skipping.")

            elif raw.startswith("+"):
                # Other LoRa module responses (e.g., +OK, +ERR)
                log(f"ℹ️  LoRa: {raw}")

    except KeyboardInterrupt:
        log("\n🛑 Gateway stopped by user.")
    except Exception as e:
        log(f"❌ Unexpected error: {e}")
    finally:
        if ser and ser.is_open:
            ser.close()
            log("Serial port closed.")


if __name__ == "__main__":
    main()
