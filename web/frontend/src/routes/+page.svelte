<script>
  import { onMount, tick, onDestroy } from 'svelte';
  import { fly, fade } from 'svelte/transition';
  // Accept route props to avoid unknown-prop warnings from SvelteKit
  // This page doesn't use route params; export a const to silence the Svelte plugin warning
  export const params = undefined;
  import Chart from 'chart.js/auto';
  import getSupabaseClient from '$lib/supabaseClient';

  const BACKEND_BASE = new URL(import.meta.env.VITE_BACKEND_URL || 'https://smartpm2-5.onrender.com');
  const MAX_RETRIES = 6;
  const RETRY_BASE_MS = 500; // ms
  
  // Supabase client for realtime subscriptions
  const supabase = getSupabaseClient();

  let latestReading = null;
  let historyData = [];
  let prevLatestId = null;
  let chart;
  let chartCanvas = null;
  let _chartCanvasAttempts = 0;
  let loadingLatest = true;
  let errorLatest = null;
  let loadingHistory = true;
  let errorHistory = null;
  let currentPeriod = 'Live';
  let realtimeSubscription = null;
  let liveDataBuffer = [];
  let isLiveMode = false;
  
  // Polling interval for non-live modes
  let pollingInterval = null;

  onMount(async () => {
    console.log('Page mounted, backend URL:', BACKEND_BASE.toString());
    await loadLatestReading();
    await loadHistory('Live'); // Default to Live view
    setupPolling();
  });

  async function loadLatestReading() {
    const prevReading = latestReading;
    loadingLatest = true;
    errorLatest = null;
    const endpoint = new URL('/api/readings/latest', BACKEND_BASE).toString();
    console.log('Loading latest reading from:', endpoint);
    let attempt = 0;
    while (attempt < MAX_RETRIES) {
      try {
        const response = await fetch(endpoint, { cache: 'no-store' });
        console.log('Response status:', response.status);
        if (!response.ok) throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        const data = await response.json();
        console.log('Data received:', data);
        
        // Smooth transition: only update if data actually changed
        const didChange = !prevReading || data.id !== prevReading.id || data.pm25 !== prevReading.pm25 || data.aqi !== prevReading.aqi;
        if (didChange) {
          latestReading = data;
          // If user is viewing a non-realtime period, refresh that historical view so new spikes show up
          // BUT don't reload if we're in Live mode since realtime subscription handles updates
          if (currentPeriod !== 'Live' && currentPeriod !== 'realtime') {
            // Prevent triggering on initial load (prevLatestId null)
            if (prevLatestId !== null && data.id !== prevLatestId) {
              // fire-and-wait so chart updates shortly after latestReading updates
              try {
                await loadHistory(currentPeriod);
              } catch (e) {
                console.warn('Refreshing history after latest reading change failed:', e);
              }
            }
          }
          prevLatestId = data.id;
        }
        errorLatest = null;
        break;
      } catch (err) {
        attempt += 1;
        console.warn(`loadLatestReading attempt ${attempt} failed:`, err.message || err);
        if (attempt >= MAX_RETRIES) {
          errorLatest = err.message || String(err);
          latestReading = null;
        } else {
          // exponential backoff
          const backoff = RETRY_BASE_MS * Math.pow(2, attempt - 1);
          await new Promise(r => setTimeout(r, backoff));
        }
      }
    }
    loadingLatest = false;
  }

  async function loadHistory(period = 'Live') {
    currentPeriod = period;
    loadingHistory = true;
    errorHistory = null;
    isLiveMode = period === 'Live';
    
    // Clean up existing realtime subscription
    if (realtimeSubscription) {
      await supabase.removeChannel(realtimeSubscription);
      realtimeSubscription = null;
    }
    
    // Force chart recreation when switching modes
    if (chart) {
      console.log('Destroying chart for mode switch');
      chart.destroy();
      chart = null;
    }
    
    let endpoint;
    
    if (period === 'Live') {
      // Live mode: Get last 5 minutes of raw data + setup realtime subscription
      endpoint = new URL(`/api/readings/latest-batch?limit=60`, BACKEND_BASE).toString(); // ~5 minutes at 5s intervals
      
      try {
        const response = await fetch(endpoint, { cache: 'no-store' });
        if (!response.ok) throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        const payload = await response.json();
        const rows = Array.isArray(payload) ? payload : (payload.data || []);
        
        // Store in live buffer and update chart
        liveDataBuffer = rows.slice(-60); // Keep last 60 points
        historyData = { data: liveDataBuffer };
        
        // Setup realtime subscription
        setupRealtimeSubscription();
        
        errorHistory = null;
        if (liveDataBuffer.length > 0) {
          await tick();
          // Small delay to ensure canvas is ready
          setTimeout(() => updateChart(), 100);
        }
      } catch (err) {
        console.error('Error loading live data:', err);
        errorHistory = err.message || String(err);
        historyData = [];
      }
    } else {
      // Aggregated modes: Use new aggregated endpoint
      endpoint = new URL(`/api/v1/readings/aggregated?timeframe=${encodeURIComponent(period)}`, BACKEND_BASE).toString();
      
      console.log('Loading aggregated history from:', endpoint);
      let attempt = 0;
      while (attempt < MAX_RETRIES) {
        try {
          const response = await fetch(endpoint, { cache: 'no-store' });
          if (!response.ok) throw new Error(`HTTP ${response.status}: ${response.statusText}`);
          const payload = await response.json();
          
          // Transform aggregated data to match chart expectations
          const aggregatedData = payload.data || [];
          const transformedData = aggregatedData.map(item => ({
            created_at: item.bucket_time,
            pm25: item.average_pm25,
            aqi: Math.round(item.average_pm25 * 4), // Simple AQI approximation
            aggregated: true
          }));
          
          historyData = { 
            data: transformedData, 
            timeframe: payload.timeframe,
            bucket_interval: payload.bucket_interval,
            aggregated: true
          };
          
          errorHistory = null;
          if (transformedData.length > 0) {
            await tick();
            updateChart();
          }
          break;
        } catch (err) {
          attempt += 1;
          console.warn(`loadHistory attempt ${attempt} failed:`, err.message || err);
          if (attempt >= MAX_RETRIES) {
            errorHistory = err.message || String(err);
            historyData = [];
          } else {
            const backoff = RETRY_BASE_MS * Math.pow(2, attempt - 1);
            await new Promise(r => setTimeout(r, backoff));
          }
        }
      }
    }
    
    loadingHistory = false;
  }

  function setupRealtimeSubscription() {
    if (!isLiveMode) return;
    
    console.log('Setting up realtime subscription for live mode');
    
    realtimeSubscription = supabase
      .channel('readings-changes')
      .on(
        'postgres_changes',
        {
          event: 'INSERT',
          schema: 'public',
          table: 'readings',
          filter: `device_id=eq.ESP32_PM25_001`
        },
        (payload) => {
          console.log('New reading received:', payload.new);
          
          if (isLiveMode && payload.new) {
            // Add new reading to live buffer
            liveDataBuffer.push(payload.new);
            
            // Keep only last 60 readings (5 minutes at 5s intervals)
            if (liveDataBuffer.length > 60) {
              liveDataBuffer = liveDataBuffer.slice(-60);
            }
            
            // Update historyData and chart - force Svelte reactivity
            historyData = { ...historyData, data: [...liveDataBuffer] };
            updateChart();
            
            // Update latest reading if this is the newest
            if (!latestReading || payload.new.id > latestReading.id) {
              latestReading = payload.new;
            }
          }
        }
      )
      .subscribe((status, err) => {
        console.log('Realtime subscription status:', status);
        if (err) {
          console.error('Realtime subscription error:', err);
        }
      });
  }

  // Try to wake the backend (Render often sleeps idle containers). This will make a single
  // request to the latest-reading endpoint; if it succeeds, we'll re-run the loaders.
  async function wakeBackend() {
    const endpoint = new URL('/api/readings/latest', BACKEND_BASE).toString();
    try {
      // fire-and-forget, but await to let the server spin up
      await fetch(endpoint, { cache: 'no-store' });
      // small delay then reload
      await new Promise(r => setTimeout(r, 1200));
      await loadLatestReading();
      await loadHistory();
    } catch (err) {
      console.warn('Wake backend attempt failed:', err.message || err);
    }
  }

  function setupPolling() {
    // Clear existing polling
    if (pollingInterval) {
      clearInterval(pollingInterval);
    }
    
    // Only poll for latest reading in non-live modes
    // Live mode gets updates via realtime subscription
    if (!isLiveMode) {
      pollingInterval = setInterval(async () => {
        await loadLatestReading();
        // Refresh aggregated data periodically for non-live modes
        if (currentPeriod !== 'Live') {
          await loadHistory(currentPeriod);
        }
      }, 30000); // Poll every 30 seconds for aggregated views
    } else {
      // In live mode, poll more frequently as fallback for realtime
      pollingInterval = setInterval(async () => {
        await loadLatestReading();
        // Also refresh live data periodically
        if (currentPeriod === 'Live') {
          await loadHistory('Live');
        }
      }, 10000); // Poll every 10 seconds in live mode as fallback
    }
  }

  // Helper function to get AQI color for charts
  function getAQIChartColor(aqi) {
    if (aqi <= 50) return { bg: 'rgba(34, 197, 94, 0.8)', border: 'rgba(34, 197, 94, 1)' }; // Green
    if (aqi <= 100) return { bg: 'rgba(251, 191, 36, 0.8)', border: 'rgba(251, 191, 36, 1)' }; // Yellow
    if (aqi <= 150) return { bg: 'rgba(249, 115, 22, 0.8)', border: 'rgba(249, 115, 22, 1)' }; // Orange
    if (aqi <= 200) return { bg: 'rgba(239, 68, 68, 0.8)', border: 'rgba(239, 68, 68, 1)' }; // Red
    return { bg: 'rgba(147, 51, 234, 0.8)', border: 'rgba(147, 51, 234, 1)' }; // Purple
  }

  // Convert UTC timestamp to local time (GMT+7)
  function formatTimeGMT7(utcTimestamp) {
    const date = new Date(utcTimestamp);
    return date.toLocaleTimeString('en-US', { 
      hour: '2-digit', 
      minute: '2-digit',
      second: '2-digit',
      hour12: false,
      timeZone: 'Asia/Bangkok'
    });
  }

  // Format last updated time
  function formatLastUpdated(utcTimestamp) {
    const date = new Date(utcTimestamp);
    return date.toLocaleString('en-US', {
      hour: '2-digit',
      minute: '2-digit', 
      second: '2-digit',
      hour12: false,
      timeZone: 'Asia/Bangkok'
    });
  }

  function updateChart() {
    if (!chartCanvas) {
      // Retry a few times in case bind:this hasn't fired yet
      if (_chartCanvasAttempts < 10) {
        _chartCanvasAttempts += 1;
        setTimeout(updateChart, 50);
        return;
      }
      console.warn('chartCanvas not ready after retries');
      return;
    }
    _chartCanvasAttempts = 0;

    const ctx = chartCanvas.getContext('2d');
    if (!ctx) {
      console.warn('Could not get canvas context');
      return;
    }

    // Get data array from historyData
    const dataArray = isLiveMode ? liveDataBuffer : (historyData.data || []);
    
    if (!dataArray || dataArray.length === 0) {
      console.warn('No data available for chart');
      return;
    }

    // Prepare data with AQI-based colors
    const chartData = dataArray.map(d => ({
      pm25: d.pm25,
      aqi: d.aqi,
      time: formatTimeGMT7(d.created_at),
      color: getAQIChartColor(d.aqi),
      aggregated: d.aggregated || false
    }));

    const chartTitle = isLiveMode ? 'PM2.5 (Live - Last 5 minutes)' : 
                      historyData.aggregated ? `PM2.5 (${currentPeriod} - ${historyData.bucket_interval} averages)` :
                      `PM2.5 (${currentPeriod})`;

        // For live mode, try to update existing chart smoothly instead of recreating
    // Only do this if we have a valid chart and we're staying in live mode
    if (chart && isLiveMode && chartData.length > 0 && currentPeriod === 'Live') {
      const newLabels = chartData.map(d => d.time);
      const newData = chartData.map(d => d.pm25 === null || d.pm25 === undefined ? 0 : d.pm25);
      const newColors = chartData.map(d => d.pm25 === null || d.pm25 === undefined ? 'rgba(148,163,184,0.08)' : d.color.bg);
      const newBorderColors = chartData.map(d => d.pm25 === null || d.pm25 === undefined ? 'rgba(148,163,184,0.12)' : d.color.border);
      
      // Update chart data smoothly
      chart.data.labels = newLabels;
      chart.data.datasets[0].data = newData;
      chart.data.datasets[0].backgroundColor = newColors;
      chart.data.datasets[0].borderColor = newBorderColors;
      
      // Use short animation for live updates
      chart.update('active');
      return;
    }

    // Always destroy and recreate chart for mode switches or initial setup
    if (chart) {
      console.log('Destroying existing chart');
      chart.destroy();
      chart = null;
    }

    console.log('Creating new chart with', chartData.length, 'data points');
    chart = new Chart(ctx, {
      type: 'bar',
      data: {
        labels: chartData.map(d => d.time),
        datasets: [{
          label: chartTitle,
          data: chartData.map(d => d.pm25 === null || d.pm25 === undefined ? 0 : d.pm25),
          backgroundColor: chartData.map(d => d.pm25 === null || d.pm25 === undefined ? 'rgba(148,163,184,0.08)' : d.color.bg),
          borderColor: chartData.map(d => d.pm25 === null || d.pm25 === undefined ? 'rgba(148,163,184,0.12)' : d.color.border),
          borderWidth: 1,
          borderRadius: 4,
          borderSkipped: false,
        }]
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
          legend: {
            display: true,
            position: 'top',
            labels: {
              font: {
                size: 12,
                weight: '500'
              },
              color: '#374151'
            }
          },
          tooltip: {
            backgroundColor: 'rgba(0, 0, 0, 0.85)',
            titleColor: '#fff',
            bodyColor: '#fff',
            borderColor: '#374151',
            borderWidth: 1,
            cornerRadius: 8,
            callbacks: {
              title: function(items) {
                return items && items.length ? items[0].label : '';
              },
              label: function(context) {
                const dataPoint = chartData[context.dataIndex] || {};
                const parts = [];
                if (dataPoint.pm25 !== null && dataPoint.pm25 !== undefined) {
                  parts.push(`PM2.5: ${dataPoint.pm25} μg/m³`);
                }
                if (dataPoint.aqi !== null && dataPoint.aqi !== undefined) {
                  parts.push(`AQI: ${dataPoint.aqi}`);
                }
                if (dataPoint.aggregated) {
                  parts.push('(aggregated)');
                }
                return parts;
              }
            }
          }
        },
        scales: {
          y: {
            beginAtZero: true,
            grid: {
              color: 'rgba(156, 163, 175, 0.2)',
              drawBorder: false
            },
            ticks: {
              color: '#6B7280',
              font: {
                size: 11
              }
            },
            title: {
              display: true,
              text: 'PM2.5 (μg/m³)',
              color: '#374151',
              font: {
                size: 12,
                weight: '500'
              }
            }
          },
          x: {
            grid: {
              display: false
            },
            ticks: {
              color: '#6B7280',
              font: {
                size: 11
              },
              maxRotation: 0
            },
            title: {
              display: true,
              text: 'Time (GMT+7)',
              color: '#374151',
              font: {
                size: 12,
                weight: '500'
              }
            }
          }
        },
        interaction: {
          intersect: false,
          mode: 'index'
        },
        animation: {
          duration: isLiveMode ? 300 : 500, // Smoother live updates
          easing: 'easeInOutQuart'
        }
      }
    });
  }

  onDestroy(async () => {
    // Cleanup chart
    if (chart) {
      try { chart.destroy(); } catch (e) { /* ignore */ }
      chart = null;
    }
    
    // Cleanup realtime subscription
    if (realtimeSubscription) {
      await supabase.removeChannel(realtimeSubscription);
      realtimeSubscription = null;
    }
    
    // Cleanup polling
    if (pollingInterval) {
      clearInterval(pollingInterval);
      pollingInterval = null;
    }
  });

  function getAQIColor(aqi) {
    if (aqi <= 50) return '#22c55e'; // Green
    if (aqi <= 100) return '#f59e0b'; // Yellow
    if (aqi <= 150) return '#f97316'; // Orange
    if (aqi <= 200) return '#ef4444'; // Red
    return '#9333ea'; // Purple
  }

  function getAQIBackgroundColor(aqi) {
    if (aqi <= 50) return '#22c55e'; // Green
    if (aqi <= 100) return '#fbbf24'; // Yellow
    if (aqi <= 150) return '#f97316'; // Orange
    if (aqi <= 200) return '#ef4444'; // Red
    return '#9333ea'; // Purple
  }

  function getHealthMessage(aqi) {
    if (aqi <= 50) return 'Good';
    if (aqi <= 100) return 'Moderate';
    if (aqi <= 150) return 'Unhealthy for Sensitive Groups';
    if (aqi <= 200) return 'Unhealthy';
    return 'Very Unhealthy';
  }

  function getFaceEmoji(aqi) {
    if (aqi <= 50) return '😊'; // Happy face for good air
    if (aqi <= 100) return '😐'; // Neutral face for moderate
    if (aqi <= 150) return '😷'; // Mask face for unhealthy for sensitive groups
    if (aqi <= 200) return '😰'; // Worried face for unhealthy
    return '🤢'; // Sick face for very unhealthy
  }
</script>

<main style="width: 100%; min-height: 100vh; margin: 0 auto; padding: 1rem; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background-color: #f8f9fa;">
  
  <!-- Responsive container that adapts to screen size -->
  <div style="width: 100%; max-width: 1200px; margin: 0 auto;">
    <!-- Header -->
    <div style="text-align: center; margin-bottom: 2rem;">
      <h1 style="font-size: clamp(1.5rem, 5vw, 2.5rem); font-weight: 600; color: #333; margin: 0;">📍 Smart PM2.5 Monitor</h1>
      {#if latestReading}
        <p style="font-size: 0.875rem; color: #666; margin: 0.5rem 0 0 0;">Last updated: {formatLastUpdated(latestReading.created_at)}</p>
      {/if}
    </div>

  <!-- Main Hero Card -->
  {#if loadingLatest}
    <div style="background: #f0f0f0; border-radius: 1rem; padding: 2rem; text-align: center; margin-bottom: 1rem; box-shadow: 0 2px 10px rgba(0,0,0,0.1);">
      <div style="font-size: 1.125rem; color: #666;">Loading...</div>
    </div>
  {:else if errorLatest}
    <div style="background: #fee; border-radius: 1rem; padding: 2rem; text-align: center; margin-bottom: 1rem; box-shadow: 0 2px 10px rgba(0,0,0,0.1);">
      <p style="color: #d32f2f; margin-bottom: 1rem;">Error: {errorLatest}</p>
      <div style="display: flex; gap: 0.75rem; justify-content: center; flex-wrap: wrap;">
        <button 
          style="padding: 0.75rem 1.5rem; background: #10b981; color: white; border: none; border-radius: 0.5rem; cursor: pointer; font-weight: 500;"
          on:click={wakeBackend}
        >
          Wake backend
        </button>
        <button 
          style="padding: 0.75rem 1.5rem; background: #3b82f6; color: white; border: none; border-radius: 0.5rem; cursor: pointer; font-weight: 500;"
          on:click={loadLatestReading}
        >
          Retry
        </button>
      </div>
    </div>
  {:else if latestReading}
    <div 
      style="background: {getAQIBackgroundColor(latestReading.aqi)}; border-radius: 1rem; padding: 2rem; margin-bottom: 1rem; box-shadow: 0 4px 20px rgba(0,0,0,0.15); position: relative; overflow: hidden; transition: background-color 0.3s ease;"
    >
      <!-- Subtle gradient overlay -->
      <div style="position: absolute; top: 0; left: 0; right: 0; bottom: 0; background: linear-gradient(135deg, rgba(255,255,255,0.1) 0%, rgba(0,0,0,0.05) 100%);"></div>
      
      <!-- Content -->
      <div style="position: relative; z-index: 2;">
        <!-- PM2.5 and Status Row -->
        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 2rem;">
          <!-- PM2.5 Value Box (main focus) -->
          <div style="background: rgba(0,0,0,0.15); padding: 0.75rem 1rem; border-radius: 0.75rem; backdrop-filter: blur(10px);">
            <div style="font-size: clamp(2rem, 6vw, 3rem); font-weight: bold; color: white; margin: 0;">{latestReading.pm25}</div>
            <div style="font-size: 0.75rem; color: rgba(255,255,255,0.9); margin: 0;">μg/m³ PM2.5</div>
          </div>
          
          <!-- Status and Face -->
          <div style="text-align: right;">
            <div style="font-size: clamp(1.25rem, 4vw, 1.5rem); font-weight: 600; color: white; margin-bottom: 0.25rem;">{getHealthMessage(latestReading.aqi)}</div>
            <div style="font-size: 2rem; margin: 0;">{getFaceEmoji(latestReading.aqi)}</div>
          </div>
        </div>

        <!-- AQI Info at bottom -->
        <div style="display: flex; justify-content: space-between; align-items: center;">
          <div style="color: rgba(255,255,255,0.95); font-size: 1rem; font-weight: 500;">Air Quality Index</div>
          <div style="color: white; font-size: 1.125rem; font-weight: 600;">{latestReading.aqi} AQI</div>
        </div>
      </div>
    </div>
  {:else}
    <div style="background: #f0f0f0; border-radius: 1rem; padding: 2rem; text-align: center; margin-bottom: 1rem; box-shadow: 0 2px 10px rgba(0,0,0,0.1);">
      <div style="font-size: 1.125rem; color: #666;">No data available</div>
    </div>
  {/if}

  <!-- Time Period Buttons -->
  <div style="background: white; border-radius: 1rem; padding: 1rem; margin-bottom: 1rem; box-shadow: 0 2px 10px rgba(0,0,0,0.08);">
    <h3 style="font-size: 1.125rem; font-weight: 600; color: #333; margin: 0 0 1rem 0;">Historical Data</h3>
    <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(60px, 1fr)); gap: 0.5rem; margin-bottom: 1rem;">
      {#each [
        { label: 'Live', value: 'Live' },
        { label: '5m', value: '5m' },
        { label: '30m', value: '30m' },
        { label: '1h', value: '1h' },
        { label: '4h', value: '4h' },
        { label: '24h', value: '24h' }
      ] as period}
        <button 
          style="padding: 0.5rem 0.75rem; background: {currentPeriod === period.value ? getAQIBackgroundColor(latestReading?.aqi || 50) : '#f1f5f9'}; color: {currentPeriod === period.value ? 'white' : '#64748b'}; border: none; border-radius: 0.5rem; cursor: pointer; font-weight: 500; font-size: 0.875rem; transition: all 0.2s ease;"
          on:click={() => loadHistory(period.value)}
        >
          {period.label}
        </button>
      {/each}
    </div>

    <!-- Chart Section -->
    {#if loadingHistory}
      <div style="text-align: center; padding: 2rem; color: #666;">
        <div style="font-size: 1rem;">Loading history...</div>
      </div>
    {:else if errorHistory}
      <div style="text-align: center; padding: 2rem; color: #d32f2f;">
        <div style="font-size: 1rem;">Error: {errorHistory}</div>
      </div>
    {:else if (isLiveMode && liveDataBuffer.length > 0) || (!isLiveMode && historyData.data && historyData.data.length > 0)}
      <div style="position: relative; height: clamp(350px, 50vh, 600px); width: 100%; background: linear-gradient(135deg, #f8fafc 0%, #f1f5f9 100%); border-radius: 1rem; padding: 1.5rem; box-shadow: 0 4px 12px rgba(0,0,0,0.08); margin: 1rem 0;">
        {#if isLiveMode}
                  {#if isLiveMode}
          <div style="position: absolute; top: 0.5rem; right: 0.5rem; background: rgba(34, 197, 94, 0.1); color: #16a34a; padding: 0.25rem 0.5rem; border-radius: 0.25rem; font-size: 0.75rem; font-weight: 500;">
            🔴 LIVE
          </div>
        {/if}
        {/if}
        <canvas bind:this={chartCanvas} id="chart" style="width: 100%; height: 100%;"></canvas>
      </div>
    {:else}
      <div style="text-align: center; padding: 2rem; color: #666;">
        <div style="font-size: 1rem;">No historical data available</div>
      </div>
    {/if}
  </div>
  </div>
</main>