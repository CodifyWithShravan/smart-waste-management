import { useState, useEffect } from "react";
import axios from "axios";
import "../styles/dataHistory.css";

const API_READINGS = "/api/readings";
const API_EXPORT = "/api/readings/export";

export default function DataHistory() {
  const [readings, setReadings] = useState([]);
  const [filterBin, setFilterBin] = useState("");
  const [loading, setLoading] = useState(true);
  const [autoRefresh, setAutoRefresh] = useState(false);

  const fetchReadings = async () => {
    try {
      const params = {};
      if (filterBin) params.binId = filterBin;
      const res = await axios.get(API_READINGS, { params });
      setReadings(res.data);
    } catch (err) {
      console.error("Error fetching readings:", err);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchReadings();
  }, [filterBin]);

  useEffect(() => {
    if (!autoRefresh) return;
    const interval = setInterval(fetchReadings, 5000);
    return () => clearInterval(interval);
  }, [autoRefresh, filterBin]);

  // Get unique bin IDs for the filter dropdown
  const uniqueBins = [...new Set(readings.map((r) => r.binId))].sort();

  const handleExport = () => {
    const params = new URLSearchParams();
    if (filterBin) params.set("binId", filterBin);
    window.open(`${API_EXPORT}?${params.toString()}`, "_blank");
  };

  const formatDate = (iso) => {
    const d = new Date(iso);
    return d.toLocaleString();
  };

  return (
    <div className="data-history">
      <div className="history-header">
        <h2>📊 Data History</h2>
        <p className="history-subtitle">
          All sensor readings stored for future analysis
        </p>
      </div>

      <div className="history-controls">
        <div className="control-group">
          <label htmlFor="bin-filter">Filter by Bin:</label>
          <select
            id="bin-filter"
            value={filterBin}
            onChange={(e) => setFilterBin(e.target.value)}
          >
            <option value="">All Bins</option>
            {uniqueBins.map((b) => (
              <option key={b} value={b}>
                {b}
              </option>
            ))}
          </select>
        </div>

        <div className="control-group">
          <label className="toggle-label">
            <input
              type="checkbox"
              checked={autoRefresh}
              onChange={(e) => setAutoRefresh(e.target.checked)}
            />
            Auto-refresh (5s)
          </label>
        </div>

        <button className="export-btn" onClick={handleExport}>
          ⬇️ Export CSV
        </button>

        <button className="refresh-btn" onClick={fetchReadings}>
          🔄 Refresh
        </button>
      </div>

      {loading ? (
        <p className="loading-text">Loading readings...</p>
      ) : readings.length === 0 ? (
        <p className="empty-text">
          No readings recorded yet. Data will appear here as your bins send
          sensor updates.
        </p>
      ) : (
        <>
          <p className="reading-count">
            Showing <strong>{readings.length}</strong> readings
          </p>
          <div className="table-wrapper">
            <table className="readings-table">
              <thead>
                <tr>
                  <th>#</th>
                  <th>Timestamp</th>
                  <th>Bin ID</th>
                  <th>Fill Level</th>
                  <th>Latitude</th>
                  <th>Longitude</th>
                </tr>
              </thead>
              <tbody>
                {readings.map((r, i) => (
                  <tr key={r._id || i}>
                    <td className="row-num">{i + 1}</td>
                    <td>{formatDate(r.timestamp)}</td>
                    <td>
                      <span className="bin-badge">{r.binId}</span>
                    </td>
                    <td>
                      <span
                        className={`fill-badge ${
                          r.fillLevel >= 80
                            ? "fill-high"
                            : r.fillLevel >= 50
                            ? "fill-med"
                            : "fill-low"
                        }`}
                      >
                        {r.fillLevel}%
                      </span>
                    </td>
                    <td>{r.latitude.toFixed(4)}</td>
                    <td>{r.longitude.toFixed(4)}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </>
      )}
    </div>
  );
}
