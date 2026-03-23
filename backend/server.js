const express = require('express');
const mongoose = require('mongoose');
const cors = require('cors');
const path = require('path');
require('dotenv').config();

const app = express();
app.use(cors());
app.use(express.json());

// ============================================================
//  Database Connection
// ============================================================

mongoose.connect(process.env.MONGO_URI)
  .then(() => console.log('✅ Connected to MongoDB successfully'))
  .catch((err) => console.error('❌ Database connection error:', err));

// ============================================================
//  Database Schema
// ============================================================

const binSchema = new mongoose.Schema({
  binId: { type: String, required: true, unique: true },
  fillLevel: { type: Number, required: true, min: 0, max: 100 },
  latitude: { type: Number, required: true },
  longitude: { type: Number, required: true },
  lastUpdated: { type: Date, default: Date.now }
});

const Bin = mongoose.model('Bin', binSchema);

// Historical readings — stores EVERY sensor reading for future analysis
const readingSchema = new mongoose.Schema({
  binId: { type: String, required: true, index: true },
  fillLevel: { type: Number, required: true, min: 0, max: 100 },
  latitude: { type: Number, required: true },
  longitude: { type: Number, required: true },
  timestamp: { type: Date, default: Date.now, index: true }
});

const Reading = mongoose.model('Reading', readingSchema);

// ============================================================
//  API Routes
// ============================================================

// Health Check — useful for monitoring and deployment verification
app.get('/api/health', (req, res) => {
  const dbStatus = mongoose.connection.readyState === 1 ? 'connected' : 'disconnected';
  res.status(200).json({
    status: 'ok',
    database: dbStatus,
    uptime: process.uptime(),
    timestamp: new Date().toISOString()
  });
});

// POST /api/bins/update — Receives data from the Raspberry Pi gateway
app.post('/api/bins/update', async (req, res) => {
  try {
    const { binId, fillLevel, latitude, longitude } = req.body;

    // Input validation
    if (!binId || fillLevel === undefined || latitude === undefined || longitude === undefined) {
      return res.status(400).json({
        message: 'Missing required fields',
        required: ['binId', 'fillLevel', 'latitude', 'longitude']
      });
    }

    if (typeof fillLevel !== 'number' || fillLevel < 0 || fillLevel > 100) {
      return res.status(400).json({
        message: 'fillLevel must be a number between 0 and 100'
      });
    }

    if (typeof latitude !== 'number' || typeof longitude !== 'number') {
      return res.status(400).json({
        message: 'latitude and longitude must be numbers'
      });
    }

    const updatedBin = await Bin.findOneAndUpdate(
      { binId: binId },
      { fillLevel, latitude, longitude, lastUpdated: Date.now() },
      { upsert: true, returnDocument: 'after' }  // Creates it if it doesn't exist
    );

    // Save historical reading for future analysis
    await Reading.create({ binId, fillLevel, latitude, longitude });

    console.log(`📥 Received update for ${binId}: ${fillLevel}% full`);
    res.status(200).json({ message: 'Success', data: updatedBin });
  } catch (error) {
    console.error('❌ Error updating bin:', error.message);
    res.status(500).json({ message: 'Server Error', error: error.message });
  }
});

// GET /api/bins — Fetches all bins (for the React dashboard)
app.get('/api/bins', async (req, res) => {
  try {
    const bins = await Bin.find().sort({ binId: 1 });
    res.status(200).json(bins);
  } catch (error) {
    res.status(500).json({ message: 'Server Error', error: error.message });
  }
});

// GET /api/bins/:binId — Fetch a single bin by its ID
app.get('/api/bins/:binId', async (req, res) => {
  try {
    const bin = await Bin.findOne({ binId: req.params.binId });

    if (!bin) {
      return res.status(404).json({ message: `Bin '${req.params.binId}' not found` });
    }

    res.status(200).json(bin);
  } catch (error) {
    res.status(500).json({ message: 'Server Error', error: error.message });
  }
});

// DELETE /api/bins/:binId — Remove a bin (for management)
app.delete('/api/bins/:binId', async (req, res) => {
  try {
    const result = await Bin.findOneAndDelete({ binId: req.params.binId });

    if (!result) {
      return res.status(404).json({ message: `Bin '${req.params.binId}' not found` });
    }

    console.log(`🗑️  Deleted bin: ${req.params.binId}`);
    res.status(200).json({ message: 'Bin deleted', data: result });
  } catch (error) {
    res.status(500).json({ message: 'Server Error', error: error.message });
  }
});

// ============================================================
//  Reading History API — For data analysis & export
// ============================================================

// GET /api/readings — Fetches historical readings with optional filters
app.get('/api/readings', async (req, res) => {
  try {
    const { binId, limit = 200, from, to } = req.query;
    const filter = {};

    if (binId) filter.binId = binId;
    if (from || to) {
      filter.timestamp = {};
      if (from) filter.timestamp.$gte = new Date(from);
      if (to) filter.timestamp.$lte = new Date(to);
    }

    const readings = await Reading.find(filter)
      .sort({ timestamp: -1 })
      .limit(parseInt(limit))
      .lean();

    res.status(200).json(readings);
  } catch (error) {
    res.status(500).json({ message: 'Server Error', error: error.message });
  }
});

// GET /api/readings/export — Download all readings as CSV
app.get('/api/readings/export', async (req, res) => {
  try {
    const { binId, from, to } = req.query;
    const filter = {};

    if (binId) filter.binId = binId;
    if (from || to) {
      filter.timestamp = {};
      if (from) filter.timestamp.$gte = new Date(from);
      if (to) filter.timestamp.$lte = new Date(to);
    }

    const readings = await Reading.find(filter)
      .sort({ timestamp: -1 })
      .lean();

    // Build CSV
    let csv = 'Timestamp,Bin ID,Fill Level (%),Latitude,Longitude\n';
    for (const r of readings) {
      const ts = new Date(r.timestamp).toISOString();
      csv += `${ts},${r.binId},${r.fillLevel},${r.latitude},${r.longitude}\n`;
    }

    res.setHeader('Content-Type', 'text/csv');
    res.setHeader('Content-Disposition', 'attachment; filename=readings_export.csv');
    res.status(200).send(csv);
  } catch (error) {
    res.status(500).json({ message: 'Server Error', error: error.message });
  }
});

// ============================================================
//  Serve Frontend Dashboard (built React app)
// ============================================================

const frontendPath = path.join(__dirname, '..', 'frontend', 'dist');
app.use(express.static(frontendPath));

// Catch-all: serve index.html for any non-API route (React SPA routing)
app.use((req, res) => {
  res.sendFile(path.join(frontendPath, 'index.html'));
});

// ============================================================
//  Start Server
// ============================================================

const PORT = process.env.PORT || 5050;
app.listen(PORT, '0.0.0.0', () => {
  console.log(`🚀 Server running on port ${PORT} (all interfaces)`);
  console.log(`🌐 Dashboard: http://localhost:${PORT}`);
});