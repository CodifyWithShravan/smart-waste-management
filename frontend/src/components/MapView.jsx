import { MapContainer, TileLayer, Marker, Popup } from "react-leaflet";
import L from "leaflet";

// Fix for Leaflet default marker icons not showing in Vite/Webpack builds
import markerIcon2x from "leaflet/dist/images/marker-icon-2x.png";
import markerIcon from "leaflet/dist/images/marker-icon.png";
import markerShadow from "leaflet/dist/images/marker-shadow.png";

delete L.Icon.Default.prototype._getIconUrl;
L.Icon.Default.mergeOptions({
  iconRetinaUrl: markerIcon2x,
  iconUrl: markerIcon,
  shadowUrl: markerShadow,
});

// Color-coded icons based on fill level
const getMarkerIcon = (fillLevel) => {
  let color = "green";
  if (fillLevel > 80) color = "red";
  else if (fillLevel > 40) color = "orange";

  return L.divIcon({
    className: "custom-marker",
    html: `<div style="
      background-color: ${color};
      width: 24px;
      height: 24px;
      border-radius: 50%;
      border: 3px solid white;
      box-shadow: 0 2px 6px rgba(0,0,0,0.4);
    "></div>`,
    iconSize: [24, 24],
    iconAnchor: [12, 12],
    popupAnchor: [0, -12],
  });
};

function MapView({ bins }) {

  // Default center: Hyderabad area (matching your bin coordinates)
  const defaultCenter = [17.4200, 78.6560];

  return (
    <MapContainer
      center={bins.length > 0 ? [bins[0].latitude, bins[0].longitude] : defaultCenter}
      zoom={15}
      style={{ height: "450px", width: "100%", marginTop: "20px", borderRadius: "12px" }}
    >

      <TileLayer
        attribution="&copy; OpenStreetMap contributors"
        url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png"
      />

      {bins.map((bin) => (
        <Marker
          key={bin._id}
          position={[bin.latitude, bin.longitude]}
          icon={getMarkerIcon(bin.fillLevel)}
        >
          <Popup>
            <b>🗑️ {bin.binId}</b>
            <br />
            Fill Level: {bin.fillLevel}%
            <br />
            Status: {bin.fillLevel > 80 ? "🔴 Full" : bin.fillLevel > 40 ? "🟠 Medium" : "🟢 Empty"}
            <br />
            <small>Updated: {new Date(bin.lastUpdated).toLocaleTimeString()}</small>
          </Popup>
        </Marker>
      ))}

    </MapContainer>
  );
}

export default MapView;