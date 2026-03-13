#!/bin/bash
# ============================================================
#  Smart Waste Management — Start All Services
# ============================================================
#  Launches the backend (which serves both the API + dashboard)
#  and the LoRa gateway script.
#
#  Usage:
#    chmod +x start_all.sh
#    ./start_all.sh
#
#  To stop: Press Ctrl+C (stops gateway), then the backend
#  will also shut down automatically.
# ============================================================

set -e

# --- Determine project paths ---
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BACKEND_DIR="$PROJECT_DIR/backend"
FRONTEND_DIR="$PROJECT_DIR/frontend"
GATEWAY_DIR="$SCRIPT_DIR"
LOG_DIR="$PROJECT_DIR/logs"

# Create logs directory
mkdir -p "$LOG_DIR"

TIMESTAMP=$(date +"%Y-%m-%d_%H-%M-%S")
BACKEND_LOG="$LOG_DIR/backend_$TIMESTAMP.log"
GATEWAY_LOG="$LOG_DIR/gateway_$TIMESTAMP.log"

# Get the Pi's LAN IP address
PI_IP=$(hostname -I 2>/dev/null | awk '{print $1}')
if [ -z "$PI_IP" ]; then
    PI_IP="localhost"
fi

echo "========================================="
echo "  🗑️  Smart Waste Management System"
echo "  Starting all services..."
echo "========================================="
echo ""

# --- Step 1: Build Frontend (if needed) ---
echo "🌐 Checking frontend build..."
cd "$FRONTEND_DIR"

# Install dependencies if missing
if [ ! -d "node_modules" ]; then
    echo "   📦 Installing frontend dependencies..."
    npm install
fi

# Build if dist doesn't exist or source is newer
if [ ! -d "dist" ]; then
    echo "   🔨 Building frontend dashboard..."
    npm run build
    echo "   ✅ Frontend built!"
else
    echo "   ✅ Frontend already built (run 'cd frontend && npm run build' to rebuild)"
fi
echo ""

# --- Step 2: Start the Backend Server (serves API + Dashboard) ---
echo "🟢 Starting Node.js backend..."
cd "$BACKEND_DIR"

# Install backend deps if missing
if [ ! -d "node_modules" ]; then
    echo "   📦 Installing backend dependencies..."
    npm install
fi

node server.js > "$BACKEND_LOG" 2>&1 &
BACKEND_PID=$!
echo "   PID: $BACKEND_PID"
echo "   Log: $BACKEND_LOG"

# Wait for backend to be ready
echo "   Waiting for backend to start..."
sleep 3

# Health check
for i in {1..10}; do
    if curl -s http://127.0.0.1:5050/api/health > /dev/null 2>&1; then
        echo "   ✅ Backend is running!"
        break
    fi
    if [ $i -eq 10 ]; then
        echo "   ❌ Backend failed to start. Check log: $BACKEND_LOG"
        kill $BACKEND_PID 2>/dev/null
        exit 1
    fi
    sleep 1
done
echo ""

# --- Step 3: Start the Gateway ---
echo "📡 Starting LoRa Gateway..."
echo "   Log: $GATEWAY_LOG"
echo ""
echo "========================================="
echo "  ✅ All services running!"
echo ""
echo "  Dashboard:  http://$PI_IP:5050"
echo "  API:        http://$PI_IP:5050/api/bins"
echo ""
echo "  Backend PID: $BACKEND_PID"
echo "========================================="
echo ""
echo "  📡 Gateway output below (Ctrl+C to stop all):"
echo "  -----------------------------------------"
echo ""

# Run gateway in foreground so Ctrl+C works
cd "$GATEWAY_DIR"

# Trap Ctrl+C to clean up all services
cleanup() {
    echo ""
    echo "🛑 Shutting down..."
    kill $BACKEND_PID 2>/dev/null && echo "   ✅ Backend stopped"
    echo "   Logs saved in: $LOG_DIR/"
    exit 0
}
trap cleanup SIGINT SIGTERM

# Start gateway (foreground — output goes to screen AND log)
# PYTHONUNBUFFERED=1 ensures output appears immediately (not buffered by pipe)
PYTHONUNBUFFERED=1 python3 gateway.py 2>&1 | tee "$GATEWAY_LOG"

# If gateway exits on its own, clean up
cleanup
