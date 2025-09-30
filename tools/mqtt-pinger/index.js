import mqtt from 'mqtt';
import { config } from 'dotenv';

config();

// Configuration from environment variables
const MQTT_BROKER_URL = process.env.MQTT_BROKER_URL || 'mqtts://your-hivemq-cluster.s2.eu.hivemq.cloud:8883';
const MQTT_USERNAME = process.env.MQTT_USERNAME;
const MQTT_PASSWORD = process.env.MQTT_PASSWORD;
const MQTT_TOPIC = process.env.MQTT_TOPIC || 'smartpm25.sensor.data';
const BACKEND_URL = process.env.BACKEND_URL || 'https://smartpm2-5.onrender.com';
const PING_ENDPOINT = process.env.PING_ENDPOINT || '/api/readings/latest';
const WAKE_TOKEN = process.env.WAKE_TOKEN; // Optional auth token
const DEBOUNCE_MS = parseInt(process.env.DEBOUNCE_MS || '30000'); // 30 seconds default
const MAX_PING_RETRIES = parseInt(process.env.MAX_PING_RETRIES || '3');

// Rate limiting per device
const lastPingTime = new Map();

class MQTTPinger {
  constructor() {
    this.client = null;
    this.isConnected = false;
    this.reconnectAttempts = 0;
    this.maxReconnectAttempts = 10;
  }

  async start() {
    console.log('🚀 Starting MQTT Pinger...');
    console.log(`📡 Broker: ${MQTT_BROKER_URL}`);
    console.log(`📋 Topic: ${MQTT_TOPIC}`);
    console.log(`🎯 Backend: ${BACKEND_URL}${PING_ENDPOINT}`);
    console.log(`⏱️  Debounce: ${DEBOUNCE_MS}ms`);

    this.connect();
  }

  connect() {
    const options = {
      username: MQTT_USERNAME,
      password: MQTT_PASSWORD,
      clientId: `mqtt-pinger-${Math.random().toString(16).substr(2, 8)}`,
      clean: true,
      reconnectPeriod: 5000,
      connectTimeout: 30000,
      rejectUnauthorized: true
    };

    console.log(`🔌 Connecting to ${MQTT_BROKER_URL}...`);
    this.client = mqtt.connect(MQTT_BROKER_URL, options);

    this.client.on('connect', () => {
      console.log('✅ Connected to MQTT broker');
      this.isConnected = true;
      this.reconnectAttempts = 0;
      
      this.client.subscribe(MQTT_TOPIC, (err) => {
        if (err) {
          console.error('❌ Failed to subscribe to topic:', err);
        } else {
          console.log(`🎧 Subscribed to topic: ${MQTT_TOPIC}`);
        }
      });
    });

    this.client.on('message', (topic, message) => {
      this.handleMessage(topic, message);
    });

    this.client.on('error', (error) => {
      console.error('❌ MQTT Connection error:', error.message);
      this.isConnected = false;
    });

    this.client.on('offline', () => {
      console.log('📴 MQTT client went offline');
      this.isConnected = false;
    });

    this.client.on('reconnect', () => {
      this.reconnectAttempts++;
      console.log(`🔄 Reconnecting to MQTT broker (attempt ${this.reconnectAttempts})...`);
      
      if (this.reconnectAttempts > this.maxReconnectAttempts) {
        console.error('❌ Max reconnection attempts reached. Exiting...');
        process.exit(1);
      }
    });

    this.client.on('close', () => {
      console.log('🔌 MQTT connection closed');
      this.isConnected = false;
    });
  }

  async handleMessage(topic, message) {
    try {
      const payload = JSON.parse(message.toString());
      const deviceId = payload.device_id;
      const timestamp = payload.timestamp;

      console.log(`📨 Message from ${deviceId}: PM2.5=${payload.pm25}, AQI=${payload.aqi}`);

      // Check if we should ping (debounce per device)
      if (this.shouldPing(deviceId)) {
        await this.pingBackend(deviceId, payload);
      } else {
        const timeSinceLastPing = Date.now() - (lastPingTime.get(deviceId) || 0);
        const remainingDebounce = Math.max(0, DEBOUNCE_MS - timeSinceLastPing);
        console.log(`⏳ Skipping ping for ${deviceId} (debounce: ${Math.round(remainingDebounce/1000)}s remaining)`);
      }
    } catch (error) {
      console.error('❌ Error parsing message:', error.message);
      console.log('Raw message:', message.toString());
    }
  }

  shouldPing(deviceId) {
    const now = Date.now();
    const lastPing = lastPingTime.get(deviceId) || 0;
    return (now - lastPing) >= DEBOUNCE_MS;
  }

  async pingBackend(deviceId, payload) {
    const url = `${BACKEND_URL}${PING_ENDPOINT}`;
    const headers = {
      'User-Agent': 'MQTT-Pinger/1.0',
      'Cache-Control': 'no-cache'
    };

    if (WAKE_TOKEN) {
      headers['X-Wake-Token'] = WAKE_TOKEN;
    }

    let attempt = 0;
    while (attempt < MAX_PING_RETRIES) {
      try {
        const startTime = Date.now();
        const response = await fetch(url, {
          method: 'GET',
          headers,
          timeout: 10000
        });

        const duration = Date.now() - startTime;
        
        if (response.ok) {
          console.log(`✅ Pinged backend for ${deviceId} (${response.status} in ${duration}ms)`);
          lastPingTime.set(deviceId, Date.now());
          return;
        } else {
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }
      } catch (error) {
        attempt++;
        console.warn(`⚠️  Ping attempt ${attempt} failed for ${deviceId}:`, error.message);
        
        if (attempt >= MAX_PING_RETRIES) {
          console.error(`❌ All ping attempts failed for ${deviceId}`);
          return;
        }
        
        // Exponential backoff
        const backoffMs = Math.min(1000 * Math.pow(2, attempt - 1), 5000);
        await new Promise(resolve => setTimeout(resolve, backoffMs));
      }
    }
  }

  async stop() {
    console.log('🛑 Stopping MQTT Pinger...');
    if (this.client) {
      this.client.end(false, () => {
        console.log('✅ MQTT client disconnected');
      });
    }
  }
}

// Handle graceful shutdown
const pinger = new MQTTPinger();

process.on('SIGINT', async () => {
  console.log('\\n🛑 Received SIGINT. Shutting down gracefully...');
  await pinger.stop();
  process.exit(0);
});

process.on('SIGTERM', async () => {
  console.log('\\n🛑 Received SIGTERM. Shutting down gracefully...');
  await pinger.stop();
  process.exit(0);
});

// Start the pinger
pinger.start().catch(error => {
  console.error('❌ Failed to start MQTT Pinger:', error);
  process.exit(1);
});