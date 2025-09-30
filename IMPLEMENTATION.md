# Time-Series Chart Refactoring - Database-Side Aggregation

This implementation refactors the time-series chart to use database-side aggregation for improved performance and scalability.

## Overview

The system now supports two types of data views:

1. **Live Mode**: Real-time data with Supabase realtime subscriptions
2. **Aggregated Views**: Pre-processed time-bucketed data (5m, 30m, 1h, 4h, 24h)

## Implementation

### 1. Database Function (Supabase)

**File**: `sql/aggregation_function.sql`

Created PostgreSQL functions for time-based aggregation:
- `get_aggregated_pm25()` - Main aggregation function with flexible bucket intervals
- `get_aggregated_pm25_simple()` - Simplified version with minute-based buckets

**To deploy**: Run the SQL file in your Supabase SQL editor.

### 2. Backend API (FastAPI)

**File**: `web/backend/main.py`

**New Endpoint**: `GET /api/v1/readings/aggregated`

Query Parameters:
- `timeframe`: "5m", "30m", "1h", "4h", "24h"

Features:
- Calls Supabase RPC function for database-side aggregation
- Fallback aggregation if database function is unavailable
- Configurable bucket intervals per timeframe
- Error handling and performance monitoring

### 3. Frontend (Svelte)

**File**: `web/frontend/src/routes/+page.svelte`

**New Features**:
- **Live Mode**: Real-time updates via Supabase realtime subscriptions
- **Aggregated Views**: Uses new `/api/v1/readings/aggregated` endpoint
- **Smart Polling**: Different polling strategies for live vs aggregated views
- **Chart Enhancements**: Shows aggregation type and optimized animations

**Live Mode Implementation**:
- Fetches last 5 minutes of raw data on load
- Subscribes to real-time database changes
- Maintains rolling buffer of 60 data points
- Automatic cleanup of subscriptions

## Configuration

### Frontend Environment Variables

Create `.env.local` in `web/frontend/`:

```bash
# Backend API URL
VITE_BACKEND_URL=https://your-backend.onrender.com

# Supabase Configuration (for realtime)
VITE_SUPABASE_URL=your_supabase_project_url
VITE_SUPABASE_ANON_KEY=your_supabase_anon_key
```

### Bucket Intervals

Default bucket intervals per timeframe:
- **5m**: 10 seconds
- **30m**: 1 minute
- **1h**: 2 minutes
- **4h**: 10 minutes
- **24h**: 30 minutes

## Performance Benefits

1. **Reduced Network Traffic**: 
   - 24h view: ~50 aggregated points vs 17,000+ raw points
   - 90%+ reduction in data transfer

2. **Faster Load Times**:
   - Database-side aggregation leverages SQL optimization
   - Minimal client-side processing

3. **Scalable Architecture**:
   - Database handles aggregation efficiently
   - Frontend focuses on visualization

4. **Real-time Efficiency**:
   - Live mode uses WebSocket-style subscriptions
   - No polling overhead for real-time updates

## Migration Notes

- Existing endpoints remain functional for backward compatibility
- New aggregated endpoint provides enhanced performance
- Live mode replaces previous "realtime" polling approach
- Chart animations optimized for different data types

## Troubleshooting

### Database Function Issues
If aggregation function deployment fails:
1. Check Supabase permissions
2. Ensure `readings` table exists
3. Verify PostgreSQL version compatibility
4. Use fallback aggregation (automatic)

### Realtime Subscription Issues
If live mode doesn't update:
1. Verify Supabase environment variables
2. Check database RLS policies
3. Ensure `readings` table has realtime enabled
4. Monitor browser console for WebSocket errors

### Performance Monitoring
- Backend logs aggregation performance
- Frontend console shows data fetch timing
- Chart rendering optimized per mode

## UI/UX Improvements Checklist

### Chart Enhancements ✅
- [x] **Live Mode Real-time Updates**: Implemented Supabase realtime subscriptions for instant data updates
- [x] **Smooth Chart Animations**: Added optimized animations for live data updates (800ms duration, easeInOutCubic)
- [x] **Mobile Responsive Charts**: Automatic data aggregation on mobile for better readability (>20 points → ~20 bars)
- [x] **Stable Layout**: Fixed chart container sizing to prevent page "bumping" during updates
- [x] **Global Box-sizing**: Applied `box-sizing: border-box` to fix mobile padding issues

### Data Management ✅
- [x] **Polling Optimization**: 5-second polling for live mode, 30-second for aggregated views
- [x] **Lightweight Live Updates**: Separate `refreshLiveData()` function to avoid chart recreation
- [x] **Train-like Scrolling**: Implemented shift-and-push pattern for smooth data flow visualization
- [x] **Memory Management**: Rolling buffer of 60 data points for live mode

### Performance Optimizations ✅
- [x] **Chart Update Strategy**: Smart chart recreation only on timeframe switches, smooth updates for live data
- [x] **Realtime Subscription Management**: Proper cleanup and recreation of Supabase subscriptions
- [x] **Error Handling**: Robust error handling for network issues and failed updates

### Known Issues 🔧
- [ ] **Layout Shift**: Chart container still causes minor page bumping (padding-top trick needs refinement)
- [ ] **Line Chart**: Removed due to visual issues - may revisit with different approach

### Future Enhancements 📋
- [ ] **Advanced Animations**: Explore Chart.js animation callbacks for smoother transitions
- [ ] **Layout Stability**: Investigate CSS containment or transform-based solutions
- [ ] **Trend Visualization**: Alternative to line chart overlay (gradient fills, sparklines)
- [ ] **Touch Interactions**: Enhanced mobile chart interactions and gestures

## Testing

Test all timeframe modes:
1. **Live**: Verify real-time updates and rolling buffer
2. **5m-1h**: Check sub-hour aggregation accuracy
3. **4h-24h**: Verify long-term aggregation performance
4. **Error Handling**: Test with network issues and invalid timeframes

## Future Enhancements

1. **Caching Layer**: Redis cache for aggregated results
2. **Materialized Views**: Pre-computed aggregations for common timeframes
3. **Advanced Analytics**: Trend analysis and forecasting
4. **Custom Timeframes**: User-defined aggregation periods