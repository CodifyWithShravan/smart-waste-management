# 🏗️ Multi-Bin Setup Guide — 3 Dustbins + Raspberry Pi Central Unit

> **This guide covers running 3 sensor-equipped dustbins together, all reporting to a single Raspberry Pi.**
> For single-bin setup basics, see [HARDWARE_SETUP_GUIDE.md](./HARDWARE_SETUP_GUIDE.md).

---

## 📐 System Architecture

```
  ┌──────────────┐     ┌──────────────┐     ┌──────────────┐
  │  BIN-001     │     │  BIN-002     │     │  BIN-003     │
  │  ESP32       │     │  ESP32       │     │  ESP32       │
  │  + Ultrasonic│     │  + Ultrasonic│     │  + Ultrasonic│
  │  + GPS       │     │  + GPS       │     │  + GPS       │
  │  + LoRa TX   │     │  + LoRa TX   │     │  + LoRa TX   │
  │  Addr: 1     │     │  Addr: 2     │     │  Addr: 3     │
  └──────┬───────┘     └──────┬───────┘     └──────┬───────┘
         │LoRa 433MHz        │LoRa 433MHz        │LoRa 433MHz
         └────────────┬──────┴──────┬─────────────┘
                      ▼             ▼
              ┌──────────────────────────┐
              │  RECEIVER ESP32          │
              │  + LoRa RX (Addr: 0)     │
              │  Network ID: 18          │
              └──────────┬───────────────┘
                         │ USB Serial
                         ▼
              ┌──────────────────────────┐
              │  RASPBERRY PI            │
              │  ├─ gateway.py           │
              │  ├─ Node.js backend      │
              │  └─ MongoDB (cloud)      │
              └──────────┬───────────────┘
                         │ HTTP API
                         ▼
              ┌──────────────────────────┐
              │  Web Dashboard           │
              │  (React + Leaflet Map)   │
              └──────────────────────────┘
```

**Key point**: All 3 senders share the **same LoRa frequency (433 MHz)** and **Network ID (18)**. The receiver at address `0` picks up ALL messages. Each bin is identified by its unique `BIN_ID` in the CSV payload.

---

## 📦 Hardware Shopping List (for 3 bins)

### Sender Side (×3 — one per bin)
| Component | Qty | Per-bin Cost (approx) |
|-----------|-----|-----------------------|
| ESP32 DOIT DevKit V1 | 3 | ₹450 |
| AJ-SR04 Ultrasonic Sensor | 3 | ₹100 |
| NEO-6M GPS Module | 3 | ₹350 |
| REYAX RYLR998 LoRa Module | 3 | ₹800 |
| Jumper Wires (M-F) | ~36 | ₹50 |
| USB Micro cables | 3 | ₹50 |

### Receiver + Central Unit (×1)
| Component | Qty |
|-----------|-----|
| ESP32 DOIT DevKit V1 | 1 |
| REYAX RYLR998 LoRa Module | 1 |
| Raspberry Pi (any model) | 1 |
| USB Micro cable | 1 |
| Pi power supply | 1 |

---

## 🔧 PHASE 1: Wire All 3 Sender Units

> Each sender has **identical wiring**. Build the same circuit 3 times.

### Wiring Diagram (same for ALL 3 bins)

```
Ultrasonic (AJ-SR04)         GPS (NEO-6M)              LoRa (RYLR998)
═══════════════════          ════════════               ═══════════════
  VCC  → 5V                    VCC  → 3.3V ⚠️           VCC  → 3.3V ⚠️
  GND  → GND                  GND  → GND               GND  → GND
  TRIG → GPIO 5               TX   → GPIO 16            TX   → GPIO 26
  ECHO → GPIO 18              RX   → GPIO 17            RX   → GPIO 27
```

> [!CAUTION]
> GPS and LoRa modules MUST use **3.3V**. Only the ultrasonic sensor uses 5V!

### Wiring Checklist (do this for each of the 3 ESP32s)

- [ ] Ultrasonic: VCC→5V, GND→GND, TRIG→GPIO5, ECHO→GPIO18
- [ ] GPS: VCC→3.3V, GND→GND, TX→GPIO16, RX→GPIO17
- [ ] LoRa: VCC→3.3V, GND→GND, TX→GPIO26, RX→GPIO27
- [ ] No loose connections, no crossed wires

---

## 🔌 PHASE 2: Wire the Receiver

Same as single-bin setup. Only 4 wires:

```
RYLR998 LoRa              ESP32 (Receiver)
════════════               ════════════════
    VCC     ──────────→  3.3V
    GND     ──────────→  GND
    TX      ──────────→  GPIO 26
    RX      ──────────→  GPIO 27
```

Plug receiver ESP32 into Raspberry Pi via USB.

---

## 💾 PHASE 3: Flash the Firmware

### ⚡ Flash each sender ESP32 with the correct firmware

You MUST flash each ESP32 from its **own directory** so it gets the right Bin ID and LoRa address.

```
┌───────────┬────────────────────────────────────┬─────────────┬──────────────┐
│ Bin       │ Firmware Directory                 │ BIN_ID      │ LORA_ADDRESS │
├───────────┼────────────────────────────────────┼─────────────┼──────────────┤
│ Dustbin 1 │ hardware/                          │ BIN-001     │ 1            │
│ Dustbin 2 │ hardware/bin-002/                  │ BIN-002     │ 2            │
│ Dustbin 3 │ hardware/bin-003/                  │ BIN-003     │ 3            │
│ Receiver  │ hardware/receiver/                 │ (N/A)       │ 0            │
└───────────┴────────────────────────────────────┴─────────────┴──────────────┘
```

### Step-by-step for each sender:

**Sender 1 (BIN-001):**
```bash
cd smart-waste-management/hardware
pio run --target upload
pio device monitor --baud 115200    # Verify: should say "Bin ID: BIN-001"
```

**Sender 2 (BIN-002):**
```bash
cd smart-waste-management/hardware/bin-002
pio run --target upload
pio device monitor --baud 115200    # Verify: should say "Bin ID: BIN-002"
```

**Sender 3 (BIN-003):**
```bash
cd smart-waste-management/hardware/bin-003
pio run --target upload
pio device monitor --baud 115200    # Verify: should say "Bin ID: BIN-003"
```

**Receiver:**
```bash
cd smart-waste-management/hardware/receiver
pio run --target upload
pio device monitor --baud 115200    # Verify: should say "Bridge is READY"
```

> [!IMPORTANT]
> Flash them **one at a time**! Plug in one ESP32, flash it, unplug it, label it (BIN-001/002/003), then do the next.

---

## 🍓 PHASE 4: Set Up the Raspberry Pi

### Step 4.1 — First-Time Setup (run once)

Copy the project to the Pi (either via SCP or git clone):

```bash
# From your laptop:
scp -r smart-waste-management/ pi@<pi-ip>:~/

# OR on the Pi:
git clone <your-repo-url>
cd smart-waste-management
```

Run the setup script:
```bash
cd gateway
chmod +x pi_setup.sh start_all.sh
sudo ./pi_setup.sh
```

This installs: Python packages, Node.js, npm dependencies, and configures serial port access.

> [!NOTE]
> If this is the first time, **reboot the Pi** after setup so the serial port permissions take effect: `sudo reboot`

### Step 4.2 — Configure the Backend

Edit `backend/.env` to set your MongoDB connection string:
```bash
nano ~/smart-waste-management/backend/.env
```

It should contain:
```
MONGO_URI=mongodb+srv://your-connection-string
PORT=5050
```

### Step 4.3 — Plug In the Receiver ESP32

Plug the receiver ESP32 into any USB port on the Pi. Verify it's detected:
```bash
ls /dev/ttyUSB* /dev/ttyACM*
```

You should see `/dev/ttyUSB0` or `/dev/ttyACM0`.

---

## 🚀 PHASE 5: Launch Everything

### One Command to Start All Services

```bash
cd ~/smart-waste-management/gateway
./start_all.sh
```

This will:
1. ✅ Start the Node.js backend
2. ✅ Wait for backend health check
3. ✅ Start the LoRa gateway
4. ✅ Begin listening for LoRa messages from all 3 bins

### What You'll See

```
=========================================
  🗑️  Smart Waste Management System
  Starting all services...
=========================================

🟢 Starting Node.js backend...
   ✅ Backend is running!

📡 Starting LoRa Gateway...

=========================================
  All services running!
  Backend PID: 1234
  Dashboard:   http://192.168.1.100:5173
  API:         http://127.0.0.1:5050/api/bins
=========================================
```

---

## 🔋 PHASE 6: Power On the Senders

1. Power each sender ESP32 via USB power bank or wall adapter
2. Wait ~10 seconds for them to boot and configure LoRa
3. Watch the gateway terminal — within 30 seconds you should see:

```
[2026-03-12 15:30:00] 📡 Received: +RCV=1,25,BIN-001,42,17.4200,78.6560,-45,12
[2026-03-12 15:30:00] ✅ Sent BIN-001 → 42% full

==============================================================
  📊 MULTI-BIN STATUS DASHBOARD
==============================================================
  Bin ID     Fill     Signal       Last Seen          Status
  ----------------------------------------------------------
  BIN-001    42%      -45dBm       3s ago             🟢 OK
  BIN-002    ---      ---          never              ⏳ WAITING
  BIN-003    ---      ---          never              ⏳ WAITING
==============================================================
```

As each sender powers up, it will appear in the dashboard!

---

## 🎯 Demo Day Quick-Start Checklist

```
 ✅ Step 1:  Power on Raspberry Pi
 ✅ Step 2:  SSH into Pi (or connect monitor)
 ✅ Step 3:  cd ~/smart-waste-management/gateway && ./start_all.sh
 ✅ Step 4:  Open web dashboard on your laptop browser
 ✅ Step 5:  Power on Sender 1 (BIN-001)
 ✅ Step 6:  Power on Sender 2 (BIN-002)
 ✅ Step 7:  Power on Sender 3 (BIN-003)
 ✅ Step 8:  Wait 30 seconds — all 3 bins should appear on dashboard!
```

### Dashboard URL
- From the Pi itself: `http://localhost:5173`
- From another device on the same network: `http://<pi-ip>:5173`

---

## 🔧 Troubleshooting — Multi-Bin Specific

| Problem | Solution |
|---------|----------|
| Only 1 of 3 bins showing data | Check the other sender ESP32s are powered on. Check serial monitor for LoRa errors. |
| All bins show same BIN_ID | You flashed the same firmware on all 3! Re-flash each from the correct directory. |
| Gateway shows "STALE" for a bin | That bin hasn't sent data in >2 minutes. Check its power and LoRa connection. |
| `start_all.sh` says "backend failed" | Check `backend/.env` has a valid `MONGO_URI`. Check internet for cloud MongoDB. |
| LoRa collisions (garbled messages) | Normal if 2 bins transmit at the exact same moment. The 30-second interval makes this rare. |
| `Permission denied: /dev/ttyUSB0` | Run `sudo usermod -aG dialout $USER` and reboot the Pi. |

---

## 📁 Project Structure (After Setup)

```
smart-waste-management/
├── backend/
│   ├── server.js          ← Node.js API (handles all bins)
│   └── .env               ← MongoDB connection
├── frontend/
│   └── src/
│       ├── App.jsx         ← Dashboard (renders all bins dynamically)
│       └── components/
│           ├── DustbinCard.jsx
│           └── MapView.jsx
├── gateway/
│   ├── gateway.py          ← LoRa gateway + multi-bin status tracker
│   ├── mock_gateway.py     ← Simulator (for testing without hardware)
│   ├── pi_setup.sh         ← One-time Pi setup
│   └── start_all.sh        ← Launch all services
├── hardware/
│   ├── src/main.cpp        ← BIN-001 firmware (LoRa addr: 1)
│   ├── platformio.ini
│   ├── bin-002/
│   │   ├── src/main.cpp    ← BIN-002 firmware (LoRa addr: 2)
│   │   └── platformio.ini
│   ├── bin-003/
│   │   ├── src/main.cpp    ← BIN-003 firmware (LoRa addr: 3)
│   │   └── platformio.ini
│   └── receiver/
│       ├── src/main.cpp    ← Receiver bridge (LoRa addr: 0)
│       └── platformio.ini
├── HARDWARE_SETUP_GUIDE.md ← Single-bin guide
└── MULTI_BIN_SETUP_GUIDE.md ← This file!
```
