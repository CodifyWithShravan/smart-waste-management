#!/bin/bash
# ============================================================
#  Smart Waste Management — Raspberry Pi One-Time Setup
# ============================================================
#  Run this ONCE on a fresh Raspberry Pi to install all
#  dependencies and configure the system.
#
#  Usage:
#    chmod +x pi_setup.sh
#    sudo ./pi_setup.sh
# ============================================================

set -e

echo "========================================="
echo "  Smart Waste Management — Pi Setup"
echo "========================================="
echo ""

# --- Step 1: System Update ---
echo "📦 Step 1/5: Updating system packages..."
sudo apt update && sudo apt upgrade -y
echo "✅ System updated"
echo ""

# --- Step 2: Install Python Dependencies ---
echo "🐍 Step 2/5: Installing Python dependencies..."
sudo apt install python3-pip python3-serial -y
pip3 install pyserial requests --break-system-packages 2>/dev/null || pip3 install pyserial requests
echo "✅ Python dependencies installed"
echo ""

# --- Step 3: Install Node.js (if not present) ---
echo "🟢 Step 3/5: Checking Node.js..."
if command -v node &> /dev/null; then
    NODE_VERSION=$(node --version)
    echo "   Node.js already installed: $NODE_VERSION"
else
    echo "   Installing Node.js..."
    curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -
    sudo apt install -y nodejs
    echo "   ✅ Node.js installed: $(node --version)"
fi
echo ""

# --- Step 4: Install Backend Dependencies ---
echo "📡 Step 4/5: Installing backend npm packages..."
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BACKEND_DIR="$PROJECT_DIR/backend"

if [ -d "$BACKEND_DIR" ]; then
    cd "$BACKEND_DIR"
    npm install
    echo "✅ Backend dependencies installed"
else
    echo "⚠️  Backend directory not found at $BACKEND_DIR"
    echo "   Make sure the full project is cloned on the Pi."
fi
echo ""

# --- Step 5: Add user to dialout group (for serial port access) ---
echo "🔌 Step 5/5: Configuring serial port access..."
sudo usermod -aG dialout $USER 2>/dev/null || true
echo "✅ User '$USER' added to dialout group"
echo ""

# --- Done ---
echo "========================================="
echo "  ✅ Setup Complete!"
echo "========================================="
echo ""
echo "  Next steps:"
echo "  1. Plug in the receiver ESP32 via USB"
echo "  2. Run:  ./start_all.sh"
echo ""
echo "  ⚠️  If this is the first time, please REBOOT"
echo "     so the dialout group change takes effect:"
echo "     sudo reboot"
echo ""
