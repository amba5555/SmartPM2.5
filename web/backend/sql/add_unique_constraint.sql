-- Make readings unique per device + timestamp to prevent duplicate inserts
ALTER TABLE public.readings
ADD CONSTRAINT readings_device_timestamp_unique UNIQUE (device_id, timestamp);
