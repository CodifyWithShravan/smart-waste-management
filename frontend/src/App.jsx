import { useState, useEffect } from "react";
import "./styles/dashboard.css";
import DustbinCard from "./components/DustbinCard";
import MapView from "./components/MapView";
import axios from "axios";

// Use 127.0.0.1 instead of localhost to avoid Mac network routing bugs
const API_URL = "http://127.0.0.1:5050/api/bins";

function App() {

  const [bins, setBins] = useState([]);

  const fetchBins = async () => {
    try {
      const response = await axios.get(API_URL);
      setBins(response.data);
    } catch (error) {
      console.error("Error fetching bins:", error);
    }
  };

  useEffect(() => {

    fetchBins();

    const interval = setInterval(() => {
      fetchBins();
    }, 5000); // refresh every 5 seconds

    return () => clearInterval(interval);

  }, []);

  return (
    <div className="dashboard">

      <h1 className="title">🗑️ Smart Waste Management Dashboard</h1>

      <button className="update-btn" onClick={fetchBins}>
        🔄 Refresh Data
      </button>

      {bins.length === 0 && (
        <p style={{ color: "#888", fontSize: "18px" }}>
          No bins detected yet. Waiting for sensor data...
        </p>
      )}

      <div className="bin-container">
        {bins.map((bin) => (
          <DustbinCard
            key={bin._id}
            name={bin.binId}
            level={bin.fillLevel}
            lastUpdated={bin.lastUpdated}
          />
        ))}
      </div>

      <h2 style={{ marginTop: "40px", color: "#2c3e50" }}>📍 Dustbin Locations</h2>

      <MapView bins={bins} />

    </div>
  );
}

export default App;