import requests
import time
import random

# The URL of your local Node.js backend
BACKEND_URL = 'http://127.0.0.1:5050/api/bins/update'

# Simulating 3 bins located around the campus area
BINS = [
    {"binId": "BIN-001", "lat": 17.4200, "lng": 78.6560},
    {"binId": "BIN-002", "lat": 17.4185, "lng": 78.6545},
    {"binId": "BIN-003", "lat": 17.4215, "lng": 78.6580}
]

print("Starting Hardware Simulator...")
print("Press Ctrl+C to stop.")

try:
    while True:
        for bin_data in BINS:
            # Simulate a changing waste level between 10% and 100%
            current_fill = random.randint(10, 100)
            
            payload = {
                "binId": bin_data["binId"],
                "fillLevel": current_fill,
                "latitude": bin_data["lat"],
                "longitude": bin_data["lng"]
            }
            
            try:
                # Send the fake data to the backend
                response = requests.post(BACKEND_URL, json=payload)
                if response.status_code == 200:
                    print(f"✅ Sent {bin_data['binId']} -> {current_fill}% full")
            except Exception as e:
                print(f"❌ Failed to connect to backend: {e}")
                
        # Wait 10 seconds before sending the next batch of data
        print("Waiting 10 seconds...\n")
        time.sleep(10)

except KeyboardInterrupt:
    print("\nSimulator stopped.")