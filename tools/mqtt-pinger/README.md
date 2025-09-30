# MQTT Pinger - Keep Render Backend Awake

A lightweight Node.js service that subscribes to HiveMQ MQTT topics and pings your Render backend when sensor data arrives, preventing auto-sleep.

## 🎯 Purpose

Render's free tier auto-sleeps after ~15 minutes of no HTTP activity. This service ensures your backend stays awake whenever sensors are actively publishing data to HiveMQ.

## 🔄 How It Works

1. **Subscribes** to HiveMQ topic `smartpm25.sensor.data`
2. **Receives** sensor messages (PM2.5, AQI, etc.)
3. **Pings** your Render backend via HTTP GET
4. **Debounces** pings (max 1 per device per 30 seconds)
5. **Retries** failed pings with exponential backoff

## 📦 Installation

```bash
cd tools/mqtt-pinger
npm install
```

## ⚙️ Configuration

1. Copy the example environment file:
```bash
cp .env.example .env
```

2. Edit `.env` with your credentials:
```bash
# Required: HiveMQ credentials
MQTT_BROKER_URL=mqtts://your-cluster.s2.eu.hivemq.cloud:8883
MQTT_USERNAME=your_username
MQTT_PASSWORD=your_password

# Required: Backend URL
BACKEND_URL=https://smartpm2-5.onrender.com

# Optional: Customize settings
DEBOUNCE_MS=30000        # 30 seconds between pings per device
MAX_PING_RETRIES=3       # Retry failed pings
```

## 🚀 Running

### Development
```bash
npm run dev    # Auto-restarts on file changes
```

### Production
```bash
npm start
```

## 📊 Example Output

```
🚀 Starting MQTT Pinger...
📡 Broker: mqtts://your-cluster.s2.eu.hivemq.cloud:8883
📋 Topic: smartpm25.sensor.data
🎯 Backend: https://smartpm2-5.onrender.com/api/readings/latest
⏱️  Debounce: 30000ms
🔌 Connecting to mqtts://your-cluster.s2.eu.hivemq.cloud:8883...
✅ Connected to MQTT broker
🎧 Subscribed to topic: smartpm25.sensor.data
📨 Message from ESP32_PM25_001: PM2.5=12, AQI=45
✅ Pinged backend for ESP32_PM25_001 (200 in 234ms)
```

## 🚀 Deployment Options

### Option 1: Railway (Recommended - Free Tier)
1. Fork this repo or upload code
2. Connect Railway to your GitHub
3. Set environment variables in Railway dashboard
4. Deploy! Railway keeps it running 24/7 on free tier.

### Option 2: Render Background Worker
1. Create new Render Background Worker
2. Connect to this repo
3. Set start command: `cd tools/mqtt-pinger && npm start`
4. Configure environment variables
5. Note: Requires paid plan (~$7/month)

### Option 3: VPS/Cloud Server
```bash
# Clone repo
git clone <your-repo>
cd SmartPM2.5/tools/mqtt-pinger

# Install dependencies
npm install

# Configure environment
cp .env.example .env
nano .env

# Install PM2 for process management
npm install -g pm2

# Start with PM2
pm2 start index.js --name mqtt-pinger
pm2 save
pm2 startup
```

### Option 4: Docker
```bash
# Build image
docker build -t mqtt-pinger .

# Run container
docker run -d --name mqtt-pinger --env-file .env mqtt-pinger
```

## 🔧 Environment Variables

| Variable | Required | Default | Description |
|----------|----------|---------|-------------|
| `MQTT_BROKER_URL` | ✅ | - | HiveMQ broker URL with protocol |
| `MQTT_USERNAME` | ✅ | - | HiveMQ username |
| `MQTT_PASSWORD` | ✅ | - | HiveMQ password |
| `MQTT_TOPIC` | ❌ | `smartpm25.sensor.data` | MQTT topic to subscribe |
| `BACKEND_URL` | ❌ | `https://smartpm2-5.onrender.com` | Your Render backend URL |
| `PING_ENDPOINT` | ❌ | `/api/readings/latest` | Endpoint to ping |
| `WAKE_TOKEN` | ❌ | - | Optional auth token |
| `DEBOUNCE_MS` | ❌ | `30000` | Min time between pings per device |
| `MAX_PING_RETRIES` | ❌ | `3` | Retry attempts for failed pings |

## 🛡️ Security

- Uses TLS for MQTT connection (port 8883)
- Optional `WAKE_TOKEN` for backend authentication
- Rate limiting prevents ping flooding
- Graceful error handling and reconnection

## 📈 Monitoring

The service logs all activity:
- MQTT connection status
- Message receipts from sensors
- Backend ping results and timing
- Error conditions and retries

## 🐛 Troubleshooting

### "Connection refused" or "Authentication failed"
- Check HiveMQ credentials in `.env`
- Verify broker URL includes `mqtts://` protocol
- Ensure your HiveMQ cluster is running

### "Backend ping failed"
- Check `BACKEND_URL` is correct
- Verify Render backend is deployed
- Check network connectivity

### "No messages received"
- Confirm sensors are publishing to correct topic
- Check MQTT topic name matches exactly
- Verify HiveMQ cluster has active connections

## 📝 License

MIT License - See LICENSE file for details