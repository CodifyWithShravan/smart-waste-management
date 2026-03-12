function DustbinCard({ name, level, lastUpdated }) {

  let status = "Empty";
  let color = "green";
  let statusClass = "empty";

  if (level > 80) {
    status = "Full";
    color = "red";
    statusClass = "full";
  }
  else if (level > 40) {
    status = "Medium";
    color = "orange";
    statusClass = "medium";
  }

  // Format the last updated time
  const updatedTime = lastUpdated
    ? new Date(lastUpdated).toLocaleTimeString()
    : "—";

  return (
    <div className="card">

      <div className="bin-icon">🗑️</div>

      <h3>{name}</h3>

      <div className="progress-bar">
        <div
          className="progress-fill"
          style={{
            width: `${level}%`,
            backgroundColor: color
          }}
        ></div>
      </div>

      <p>Fill Level: {level}%</p>

      <p className={`status ${statusClass}`}>
        Status: {status}
      </p>

      <p className="last-updated">
        🕐 Updated: {updatedTime}
      </p>

    </div>
  );
}

export default DustbinCard;