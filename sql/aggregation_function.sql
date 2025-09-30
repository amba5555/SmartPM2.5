-- PostgreSQL aggregation function for time-series PM2.5 data
-- This function should be executed in your Supabase SQL editor

-- First, ensure TimescaleDB extension is available (if needed for time_bucket)
-- If time_bucket is not available, we'll use a custom grouping approach

CREATE OR REPLACE FUNCTION get_aggregated_pm25(
    start_time timestamptz,
    end_time timestamptz,
    bucket_interval text
)
RETURNS TABLE (
    bucket_time timestamptz,
    average_pm25 numeric
) AS $$
DECLARE
    interval_minutes integer;
BEGIN
    -- Convert bucket_interval text to minutes for grouping
    CASE bucket_interval
        WHEN '10 seconds' THEN interval_minutes := 0; -- Use sub-minute grouping
        WHEN '1 minute' THEN interval_minutes := 1;
        WHEN '2 minutes' THEN interval_minutes := 2;
        WHEN '10 minutes' THEN interval_minutes := 10;
        WHEN '30 minutes' THEN interval_minutes := 30;
        ELSE interval_minutes := 5; -- Default to 5 minutes
    END CASE;

    IF interval_minutes = 0 THEN
        -- For sub-minute intervals, group by 10-second buckets
        RETURN QUERY
        SELECT
            date_trunc('minute', r.created_at) + 
            INTERVAL '10 seconds' * FLOOR(EXTRACT(SECOND FROM r.created_at) / 10) AS bucket_time,
            ROUND(AVG(r.pm25)::numeric, 2) AS average_pm25
        FROM
            public.readings r
        WHERE
            r.created_at >= start_time 
            AND r.created_at < end_time
            AND r.device_id != 'INTEGRATION_TEST_001'
        GROUP BY
            bucket_time
        ORDER BY
            bucket_time;
    ELSE
        -- For minute-based intervals
        RETURN QUERY
        SELECT
            date_trunc('hour', r.created_at) + 
            INTERVAL '1 minute' * (FLOOR(EXTRACT(MINUTE FROM r.created_at) / interval_minutes) * interval_minutes) AS bucket_time,
            ROUND(AVG(r.pm25)::numeric, 2) AS average_pm25
        FROM
            public.readings r
        WHERE
            r.created_at >= start_time 
            AND r.created_at < end_time
            AND r.device_id != 'INTEGRATION_TEST_001'
        GROUP BY
            bucket_time
        ORDER BY
            bucket_time;
    END IF;
END;
$$ LANGUAGE plpgsql;

-- Alternative simpler version using extract and floor for grouping
CREATE OR REPLACE FUNCTION get_aggregated_pm25_simple(
    start_time timestamptz,
    end_time timestamptz,
    bucket_minutes integer DEFAULT 5
)
RETURNS TABLE (
    bucket_time timestamptz,
    average_pm25 numeric,
    sample_count bigint
) AS $$
BEGIN
    RETURN QUERY
    SELECT
        date_trunc('hour', r.created_at) + 
        INTERVAL '1 minute' * (FLOOR(EXTRACT(MINUTE FROM r.created_at) / bucket_minutes) * bucket_minutes) AS bucket_time,
        ROUND(AVG(r.pm25)::numeric, 2) AS average_pm25,
        COUNT(*) AS sample_count
    FROM
        public.readings r
    WHERE
        r.created_at >= start_time 
        AND r.created_at < end_time
        AND r.device_id != 'INTEGRATION_TEST_001'
    GROUP BY
        bucket_time
    ORDER BY
        bucket_time;
END;
$$ LANGUAGE plpgsql;

-- Grant execute permissions
GRANT EXECUTE ON FUNCTION get_aggregated_pm25(timestamptz, timestamptz, text) TO authenticated, anon;
GRANT EXECUTE ON FUNCTION get_aggregated_pm25_simple(timestamptz, timestamptz, integer) TO authenticated, anon;