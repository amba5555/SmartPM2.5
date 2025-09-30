<script>
  import { onMount, tick, onDestroy } from 'svelte';
  import { fly, fade } from 'svelte/transition';
  // Accept route props to avoid unknown-prop warnings from SvelteKit
  // This page doesn't use route params; export a const to silence the Svelte plugin warning
  export const params = undefined;
  import Chart from 'chart.js/auto';

    const BACKEND_BASE = new URL(import.meta.env.VITE_BACKEND_URL || 'https://smartpm2-5.onrender.com'); // Replace with actual Render URL
    const MAX_RETRIES = 6;
    const RETRY_BASE_MS = 500; // ms

  let latestReading = null;
  let historyData = [];
  let chart;
  let chartCanvas = null;
  let _chartCanvasAttempts = 0;
  let loadingLatest = true;
  let errorLatest = null;
  let loadingHistory = true;
  let errorHistory = null;
  let currentPeriod = 'realtime';

  onMount(async () => {
    console.log('Page mounted, backend URL:', BACKEND_BASE.toString());
    await loadLatestReading();
    await loadHistory('realtime'); // Default to real-time rolling data
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
        if (!prevReading || data.id !== prevReading.id || data.pm25 !== prevReading.pm25 || data.aqi !== prevReading.aqi) {
          latestReading = data;
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

  async function loadHistory(period = 'realtime') {
    currentPeriod = period;
    loadingHistory = true;
    errorHistory = null;
    
    let endpoint;
    if (period === 'realtime') {
      // Real-time: Last 50 rows, rolling
      endpoint = new URL(`/api/readings/latest-batch?limit=50`, BACKEND_BASE).toString();
    } else {
      // Time-based periods
      endpoint = new URL(`/api/readings/history?period=${encodeURIComponent(period)}`, BACKEND_BASE).toString();
    }
    
    console.log('Loading history from:', endpoint);
    let attempt = 0;
    while (attempt < MAX_RETRIES) {
      try {
        const response = await fetch(endpoint, { cache: 'no-store' });
        if (!response.ok) throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        const payload = await response.json();
        // Backend returns { data: [...], count: N } — normalize to an array
        const rows = Array.isArray(payload) ? payload : (payload.data || []);
        historyData = rows;
        errorHistory = null;
        // Only try to render the chart when we actually have points
        if (historyData && historyData.length > 0) {
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
    loadingHistory = false;
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
    setInterval(async () => {
      await loadLatestReading();
      // Auto-refresh historical data if on real-time view
      if (currentPeriod === 'realtime') {
        await loadHistory('realtime');
      }
    }, 10000); // Poll every 10 seconds
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

    if (chart) chart.destroy();

    // Prepare data with AQI-based colors
    const chartData = historyData.map(d => ({
      pm25: d.pm25,
      aqi: d.aqi,
      time: formatTimeGMT7(d.created_at),
      color: getAQIChartColor(d.aqi)
    }));

    chart = new Chart(ctx, {
      type: 'bar',
      data: {
        labels: chartData.map(d => d.time),
        datasets: [{
          label: 'PM2.5 (μg/m³)',
          data: chartData.map(d => d.pm25),
          backgroundColor: chartData.map(d => d.color.bg),
          borderColor: chartData.map(d => d.color.border),
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
            backgroundColor: 'rgba(0, 0, 0, 0.8)',
            titleColor: '#fff',
            bodyColor: '#fff',
            borderColor: '#374151',
            borderWidth: 1,
            cornerRadius: 8,
            callbacks: {
              label: function(context) {
                const dataPoint = chartData[context.dataIndex];
                return [
                  `PM2.5: ${dataPoint.pm25} μg/m³`,
                  `AQI: ${dataPoint.aqi}`
                ];
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
          duration: 750,
          easing: 'easeInOutQuart'
        }
      }
    });
  }

  onDestroy(() => {
    if (chart) {
      try { chart.destroy(); } catch (e) { /* ignore */ }
      chart = null;
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

<main style="max-width: min(100vw, 1200px); width: 100%; margin: 0 auto; padding: 1rem; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background-color: #f8f9fa; display: flex; flex-direction: column; align-items: center;">
  
  <div style="width: 100%; max-width: min(400px, 100vw); margin: 0 auto;">
    <!-- Header -->
    <div style="text-align: center; margin-bottom: 2rem;">
      <h1 style="font-size: 1.5rem; font-weight: 600; color: #333; margin: 0;">Smart PM2.5 Monitor</h1>
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
      style="background: {getAQIBackgroundColor(latestReading.aqi)}; border-radius: 1rem; padding: 2rem; margin-bottom: 1rem; box-shadow: 0 4px 20px rgba(0,0,0,0.15); position: relative; overflow: hidden;"
      in:fly={{ y: 20, duration: 300 }} out:fade={{ duration: 200 }}
    >
      <!-- Subtle gradient overlay -->
      <div style="position: absolute; top: 0; left: 0; right: 0; bottom: 0; background: linear-gradient(135deg, rgba(255,255,255,0.1) 0%, rgba(0,0,0,0.05) 100%);"></div>
      
      <!-- Content -->
      <div style="position: relative; z-index: 2;">
        <!-- AQI and Status Row -->
        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 2rem;">
          <!-- AQI Box -->
          <div style="background: rgba(0,0,0,0.15); padding: 0.75rem 1rem; border-radius: 0.75rem; backdrop-filter: blur(10px);">
            <div style="font-size: 1.75rem; font-weight: bold; color: white; margin: 0;">{latestReading.aqi}</div>
            <div style="font-size: 0.75rem; color: rgba(255,255,255,0.9); margin: 0;">US AQI</div>
          </div>
          
          <!-- Status and Face -->
          <div style="text-align: right;">
            <div style="font-size: 1.5rem; font-weight: 600; color: white; margin-bottom: 0.25rem;">{getHealthMessage(latestReading.aqi)}</div>
            <div style="font-size: 2rem; margin: 0;">{getFaceEmoji(latestReading.aqi)}</div>
          </div>
        </div>

        <!-- PM2.5 Info -->
        <div style="display: flex; justify-content: space-between; align-items: center;">
          <div style="color: rgba(255,255,255,0.95); font-size: 1rem; font-weight: 500;">Main pollutant: PM2.5</div>
          <div style="color: white; font-size: 1.125rem; font-weight: 600;">{latestReading.pm25} μg/m³</div>
        </div>
      </div>
    </div>
  {:else}
    <div style="background: #f0f0f0; border-radius: 1rem; padding: 2rem; text-align: center; margin-bottom: 1rem; box-shadow: 0 2px 10px rgba(0,0,0,0.1);">
      <div style="font-size: 1.125rem; color: #666;">No data available</div>
    </div>
  {/if}

  <!-- Additional Reading Info -->
  {#if latestReading}
    <div style="background: white; border-radius: 1rem; padding: 1rem; margin-bottom: 1rem; box-shadow: 0 2px 10px rgba(0,0,0,0.08); display: grid; grid-template-columns: repeat(auto-fit, minmax(100px, 1fr)); gap: 1rem; text-align: center;">
      <div>
        <div style="font-size: 1.125rem; font-weight: 600; color: #333;">{latestReading.pm10}</div>
        <div style="font-size: 0.75rem; color: #666;">PM10 μg/m³</div>
      </div>
      <div>
        <div style="font-size: 1.125rem; font-weight: 600; color: #333;">{latestReading.pm1}</div>
        <div style="font-size: 0.75rem; color: #666;">PM1 μg/m³</div>
      </div>
      <div>
        <div style="font-size: 1.125rem; font-weight: 600; color: #333;">{latestReading.wifi_rssi} dBm</div>
        <div style="font-size: 0.75rem; color: #666;">WiFi Signal</div>
      </div>
    </div>
  {/if}

  <!-- Time Period Buttons -->
  <div style="background: white; border-radius: 1rem; padding: 1rem; margin-bottom: 1rem; box-shadow: 0 2px 10px rgba(0,0,0,0.08);">
    <h3 style="font-size: 1.125rem; font-weight: 600; color: #333; margin: 0 0 1rem 0;">Historical Data</h3>
    <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(60px, 1fr)); gap: 0.5rem; margin-bottom: 1rem;">
      {#each [
        { label: 'Live', value: 'realtime' },
        { label: '5min', value: '5min' },
        { label: '30min', value: '30min' },
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
    {:else if historyData.length > 0}
      <div style="position: relative; height: 300px; width: 100%; background: linear-gradient(135deg, #f8fafc 0%, #f1f5f9 100%); border-radius: 0.75rem; padding: 1rem; box-shadow: inset 0 1px 3px rgba(0,0,0,0.05);">
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