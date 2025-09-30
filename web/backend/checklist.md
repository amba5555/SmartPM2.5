# Backend Deployment & Run Checklist (SmartPM2.5)

Short checklist to prepare, run, and deploy the backend safely.

## Local setup
- [ ] Copy env template and fill secrets:
  - cp ../../.env.example .env
  - Edit `.env` and set SUPABASE_URL, SUPABASE_KEY, MQTT_BROKER, MQTT_USERNAME, MQTT_PASSWORD

- [ ] Create and activate venv, install deps:
  - python -m venv venv
  - source venv/bin/activate
  - pip install -r requirements.txt

- [ ] Start local backend for dev/testing (no production reload):
  - ./venv/bin/python -m uvicorn main:app --port 8000

- [ ] Run the integration test (requires backend running):
  - ./venv/bin/python -m pytest tests/test_integration.py::test_publish_and_check_api -q

## Render production checklist
- [ ] Connect GitHub repo and create Render Web Service
  - Root Directory: `web/backend`
  - Branch: `main`

- [ ] Build Command:
  - pip install -r requirements.txt

- [ ] Start Command:
  - uvicorn main:app --host 0.0.0.0 --port $PORT
  - Ensure `--reload` is NOT used in production.

- [ ] Runtime: use Python 3.11 on Render
  - Optionally set `PYTHONUNBUFFERED=1` for immediate log flush

- [ ] Environment variables (secure in Render):
  - SUPABASE_URL
  - SUPABASE_KEY (use service_role for server)
  - MQTT_BROKER
  - MQTT_USERNAME
  - MQTT_PASSWORD
  - ENABLE_MQTT_DEBUG=false
  - DEBUG_TOKEN (only if ENABLE_MQTT_DEBUG=true temporarily)
  - BACKEND_LOG_PATH (optional; defaults to ./backend.log)

- [ ] Health Check Path (Render): `/health`
- [ ] Instances: start with 1 to avoid duplicate MQTT subscribers

## Database safety (Supabase)
- [ ] Run SQL to create readings table (if not present):
  - `web/backend/sql/create_readings_table.sql`

- [ ] (Optional, recommended) Run duplicate cleanup and add unique constraint:
  - `web/backend/sql/cleanup_duplicates.sql` (if needed)
  - `web/backend/sql/add_unique_constraint.sql`

## In-code protections
- [ ] Upsert on insert is enabled (idempotent): `supabase.table('readings').upsert(payload).execute()`
- [ ] MQTT TLS is enabled via `mqtt_client.tls_set()` before connect
- [ ] Debug endpoint `/debug/mqtt` is gated by `ENABLE_MQTT_DEBUG` and `DEBUG_TOKEN`
- [ ] Logs stream to stdout so Render captures them; optional rotating file handler exists

## Quick troubleshooting
- Duplicate rows:
  - Check for multiple subscribers (local + Render) and stop local when Render is active
  - Ensure unique constraint on `(device_id, timestamp)` and use upsert
- No messages arriving:
  - Confirm device publishes to `smartpm25.sensor.data` and credentials match
  - Check Render logs for `Connected to MQTT broker` and `Subscribe result`
- Supabase insert errors:
  - Use service_role key for `SUPABASE_KEY`, check permissions, and inspect response in logs

## Files to review before deploy
- `web/backend/main.py` — core app and MQTT handling
- `web/backend/requirements.txt` — pinned deps for Python 3.11
- `web/backend/sql/*` — schema and maintenance SQL
- `web/backend/test_mqtt.py` and `web/backend/tests/test_integration.py` — test utilities
- `.gitignore` — ensures `.env` and `venv/` are not committed

## Post-deploy verification
1. Check Render logs for startup messages and MQTT subscription.
2. Publish a test message using `test_mqtt.py` and watch Render logs for `Received:` and `Supabase insert response:`.
3. Verify `/api/readings/latest` returns the new reading.

---

If you want, I can commit this file and push it to `origin/main`.
