# SmartPM2.5 Backend

This backend subscribes to the MQTT topic `smartpm25/sensor/data` and writes sensor readings to Supabase. It also exposes simple REST endpoints for the frontend.

## Setup (local)

1. Copy the environment template:

```bash
cp ../../.env.example .env
# then edit .env and fill your SUPABASE_URL, SUPABASE_KEY, MQTT_* values
```

2. Create virtual environment and install dependencies:

```bash
python -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

3. Initialize database schema in Supabase (use `web/backend/sql/create_readings_table.sql` in Supabase SQL Editor).

4. Run locally:

```bash
uvicorn main:app --reload --port 8000
```

## API Endpoints
- `GET /api/readings/latest` — returns latest reading
- `GET /api/readings/history?period=24h|7d|30d` — returns historical readings

## Debug & Tests

Enable the debug endpoint (only for local testing):

```bash
export ENABLE_MQTT_DEBUG=true
```

Logs will also be written to `./backend.log` by default. To change the path:

```bash
export BACKEND_LOG_PATH=/path/to/backend.log
```

Run integration tests (assumes backend is running and env is configured):

```bash
cd web/backend
source venv/bin/activate
pip install -r requirements.txt
pytest -q
```

