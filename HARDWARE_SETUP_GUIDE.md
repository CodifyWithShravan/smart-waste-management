# 🚀 Smart Waste Management — Complete Hardware Setup Guide

> **This guide walks you through the ENTIRE setup, step by step, baby-steps style.**
> Follow it from top to bottom and you'll have a working demo.

> [!TIP]
> **Running 3 dustbins?** See [MULTI_BIN_SETUP_GUIDE.md](./MULTI_BIN_SETUP_GUIDE.md) for the complete multi-bin deployment with a Raspberry Pi central unit.

---

## 📦 What You Need (Shopping List)

### Sender Side (goes on/near the waste bin)
| # | Component | Quantity | Notes |
|---|-----------|----------|-------|
| 1 | ESP32 DOIT DevKit V1 | 1 | The main microcontroller |
| 2 | AJ-SR04 Ultrasonic Sensor | 1 | Measures bin fill level |
| 3 | NEO-6M GPS Module | 1 | Gets bin location |
| 4 | REYAX RYLR998 LoRa Module | 1 | Sends data wirelessly |
| 5 | Jumper Wires (Male-to-Female) | ~12 | For connections |
| 6 | Breadboard | 1 | Optional but makes wiring easier |
| 7 | USB Micro cable | 1 | To flash firmware & power |

### Receiver Side (the gateway)
| # | Component | Quantity | Notes |
|---|-----------|----------|-------|
| 1 | ESP32 DOIT DevKit V1 | 1 | Bridge between LoRa and Pi |
| 2 | REYAX RYLR998 LoRa Module | 1 | Receives data wirelessly |
| 3 | Raspberry Pi (any model with USB) | 1 | Runs the gateway script |
| 4 | Jumper Wires (Male-to-Female) | ~4 | For LoRa ↔ ESP32 |
| 5 | USB Micro cable (for ESP32) | 1 | Connects ESP32 to Pi |
| 6 | Power supply for Pi | 1 | Micro-USB or USB-C depending on model |

### Tools
- A laptop with USB port (to flash the ESP32s)
- Arduino IDE or PlatformIO installed

---

## 🔌 PHASE 1: Wire Up the SENDER Side

> This ESP32 reads sensors and sends data via LoRa.

### Step 1.1 — Wire the Ultrasonic Sensor (AJ-SR04)

```
AJ-SR04 Sensor          ESP32
═══════════════          ══════
    VCC     ──────────→  5V
    GND     ──────────→  GND
    TRIG    ──────────→  GPIO 5
    ECHO    ──────────→  GPIO 18
```

> [!TIP]
> The sensor has 4 pins. Look at the labels printed on the sensor board. VCC and GND are power, TRIG triggers a ping, ECHO returns the result.

### Step 1.2 — Wire the GPS Module (NEO-6M)

```
NEO-6M GPS               ESP32
═══════════               ══════
    VCC     ──────────→  3.3V  ⚠️ NOT 5V!
    GND     ──────────→  GND
    TX      ──────────→  GPIO 16
    RX      ──────────→  GPIO 17
```

> [!CAUTION]
> The NEO-6M must be powered from **3.3V**, NOT 5V! Connecting to 5V can damage it.

> [!NOTE]
> GPS TX goes to ESP32 GPIO 16 (which is the ESP32's RX1). This is correct — TX on one side connects to RX on the other.

### Step 1.3 — Wire the LoRa Module (RYLR998)

```
RYLR998 LoRa              ESP32
════════════               ══════
    VCC     ──────────→  3.3V  ⚠️ NOT 5V!
    GND     ──────────→  GND
    TX      ──────────→  GPIO 26
    RX      ──────────→  GPIO 27
```

> [!CAUTION]
> The RYLR998 also runs on **3.3V**. Never connect to 5V!

### Step 1.4 — Double-Check Your Wiring

Before powering on, verify:
- [ ] Ultrasonic VCC → 5V, GND → GND, TRIG → GPIO 5, ECHO → GPIO 18
- [ ] GPS VCC → 3.3V, GND → GND, TX → GPIO 16, RX → GPIO 17
- [ ] LoRa VCC → 3.3V, GND → GND, TX → GPIO 26, RX → GPIO 27
- [ ] No loose connections

---

## 🔌 PHASE 2: Wire Up the RECEIVER Side

> This ESP32 just bridges the LoRa module to the Raspberry Pi. Much simpler!

### Step 2.1 — Wire the LoRa Module to the Receiver ESP32

```
RYLR998 LoRa              ESP32 (Receiver)
════════════               ════════════════
    VCC     ──────────→  3.3V
    GND     ──────────→  GND
    TX      ──────────→  GPIO 26
    RX      ──────────→  GPIO 27
```

**That's it!** Only 4 wires on the receiver side. Same pins as the sender.

### Step 2.2 — Connect ESP32 to Raspberry Pi

Simply plug a **USB Micro cable** from the receiver ESP32 into any USB port on the Raspberry Pi.

```
  [RYLR998 LoRa] ←→ [ESP32 Receiver] ═══USB═══ [Raspberry Pi]
```

---

## 💾 PHASE 3: Flash the Firmware

### Step 3.1 — Install PlatformIO (if not already done)

> On your laptop, not the Pi.

**Option A: VS Code Extension (Recommended)**
1. Open VS Code
2. Go to Extensions (Ctrl+Shift+X)
3. Search for "PlatformIO IDE"
4. Click Install
5. Wait for it to finish (takes a few minutes)

**Option B: Command Line**
```bash
pip install platformio
```

### Step 3.2 — Flash the SENDER ESP32

1. Connect the **sender ESP32** to your laptop via USB
2. Open a terminal and navigate to the project:

```bash
cd /path/to/smart-waste-management/hardware
```

3. Build and upload:

```bash
# Using PlatformIO CLI:
pio run --target upload

# OR if using VS Code PlatformIO:
# Click the → (Upload) button in the bottom toolbar
```

4. Open the Serial Monitor to verify:

```bash
pio device monitor --baud 115200
```

You should see:
```
=========================================
  Smart Waste Management - Edge Node
  Bin ID: BIN-001
=========================================
✅ Ultrasonic sensor initialized
✅ GPS serial initialized (waiting for fix...)

--- Configuring LoRa Module ---
  ✅ LoRa module detected
  Address set to 1
  Network ID set to 18
  Frequency set to 433000000 Hz
--- LoRa Configuration Done ---

✅ LoRa module ready
-----------------------------------------
Starting sensor readings...
```

> [!IMPORTANT]
> If you see `❌ LoRa not responding`, check the LoRa wiring and make sure VCC is connected to 3.3V.

5. **Disconnect** the sender ESP32 from your laptop.

### Step 3.3 — Flash the RECEIVER ESP32

1. Connect the **receiver ESP32** to your laptop via USB
2. Navigate to the receiver firmware:

```bash
cd /path/to/smart-waste-management/hardware/receiver
```

3. Build and upload:

```bash
pio run --target upload
```

4. Open Serial Monitor to verify:

```bash
pio device monitor --baud 115200
```

You should see:
```
=========================================
  Smart Waste Management
  LoRa-to-USB Bridge (Receiver)
=========================================

--- Configuring LoRa Module ---
  Testing connection... OK ✅
  Setting address to 0... OK ✅
  Setting Network ID to 18... OK ✅
  Setting frequency to 433000000 Hz... OK ✅

--- Current LoRa Configuration ---
  Address:    +ADDRESS=0
  Network ID: +NETWORKID=18
  Frequency:  +BAND=433000000
  Parameters: +PARAMETER=...
--- Configuration Complete ---

✅ Bridge is READY — Listening for LoRa messages...
```

5. **Disconnect** from laptop. Plug into the **Raspberry Pi** USB port instead.

---

## 🍓 PHASE 4: Set Up the Raspberry Pi

### Step 4.1 — Prepare the Pi

1. Boot up the Raspberry Pi (with Raspberry Pi OS installed)
2. Connect to a monitor/keyboard OR SSH in:

```bash
ssh pi@<your-pi-ip-address>
```

### Step 4.2 — Install Python Dependencies

```bash
sudo apt update
sudo apt install python3-pip -y
pip3 install pyserial requests
```

### Step 4.3 — Find the ESP32's Serial Port

After plugging the receiver ESP32 into the Pi's USB, run:

```bash
ls /dev/ttyUSB* /dev/ttyACM*
```

You'll see something like:
```
/dev/ttyUSB0
```
or
```
/dev/ttyACM0
```

> [!NOTE]
> If you see `/dev/ttyUSB0`, you're good — that's the default in `gateway.py`.  
> If you see `/dev/ttyACM0`, you'll need to update line 42 of `gateway.py`.

### Step 4.4 — Copy the Gateway Script to the Pi

From your laptop, copy the gateway script:

```bash
scp gateway/gateway.py pi@<your-pi-ip>:~/gateway.py
```

Or clone the whole repo on the Pi:
```bash
git clone <your-repo-url>
cd smart-waste-management/gateway
```

### Step 4.5 — Update the Backend URL

Edit `gateway.py` and update the `BACKEND_URL` to point to where your Node.js backend is running:

```python
# If backend runs on the SAME Pi:
BACKEND_URL = "http://127.0.0.1:5050/api/bins/update"

# If backend runs on your laptop (e.g., IP 192.168.1.100):
BACKEND_URL = "http://192.168.1.100:5050/api/bins/update"
```

### Step 4.6 — Start the Backend (if running on the Pi)

In a separate terminal/screen:

```bash
cd smart-waste-management/backend
npm install
npm start
```

### Step 4.7 — Run the Gateway Script

```bash
python3 gateway.py
```

You should see:
```
[2026-03-07 10:15:00] =========================================
[2026-03-07 10:15:00]   Smart Waste Management — LoRa Gateway
[2026-03-07 10:15:00]   Serial Port: /dev/ttyUSB0
[2026-03-07 10:15:00]   Backend URL: http://127.0.0.1:5050/api/bins/update
[2026-03-07 10:15:00] =========================================
[2026-03-07 10:15:00] ✅ Connected to /dev/ttyUSB0
[2026-03-07 10:15:00] Listening for LoRa messages...
```

---

## 🧪 PHASE 5: Test the Full Setup

### Step 5.1 — Power On the Sender

1. Power the sender ESP32 via a USB power bank or wall adapter
2. Wait ~5 seconds for it to boot and configure LoRa

### Step 5.2 — Watch the Magic Happen

On the Raspberry Pi terminal (where `gateway.py` is running), within 30 seconds you should see:

```
[2026-03-07 10:16:30] 📡 Received: +RCV=1,25,BIN-001,75,17.4200,78.6560,-45,12
[2026-03-07 10:16:30]    Signal: RSSI=-45dBm, SNR=12dB
[2026-03-07 10:16:30] ✅ Sent BIN-001 → 75% full
```

### Step 5.3 — Check the Frontend

Open the web dashboard and verify the bin data is updating in real-time!

---

## 🎯 Demo Day Quick Reference

During the demo, here's the order of operations:

```
1. Power on Raspberry Pi
2. Start the backend server (npm start)
3. Open the frontend dashboard in a browser
4. Run gateway.py on the Pi
5. Power on the sender ESP32 (plug in USB power)
6. Wait 30 seconds — data should appear on dashboard!
```

---

## 🔧 Troubleshooting

| Problem | What to Check |
|---------|---------------|
| `❌ Serial error` on gateway.py | Wrong port name. Run `ls /dev/ttyUSB*` and update `SERIAL_PORT` |
| `❌ LoRa not responding` | Check VCC is on 3.3V. Check TX/RX wires aren't swapped |
| No data received | Make sure both LoRa modules use same Network ID (18) and Frequency (433 MHz) |
| `❌ Cannot connect to backend` | Backend not running, or wrong IP in `BACKEND_URL` |
| GPS shows 0,0 | Normal! GPS takes 2-5 minutes for first fix outdoors. Indoors it may never get a fix |
| Ultrasonic shows SENSOR ERROR | Check TRIG→GPIO 5 and ECHO→GPIO 18 wiring |
| ESP32 not detected on laptop | Try a different USB cable (some are charge-only!) |
| `/dev/ttyACM0` instead of `ttyUSB0` | Some ESP32 boards use ACM. Just update `SERIAL_PORT` in gateway.py |
