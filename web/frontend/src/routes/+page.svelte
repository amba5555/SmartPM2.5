<script>
  import { onMount, tick, onDestroy } from 'svelte';
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

  onMount(async () => {
    console.log('Page mounted, backend URL:', BACKEND_BASE.toString());
    await loadLatestReading();
    await loadHistory('realtime'); // Default to real-time rolling data
    setupPolling();
  });

  async function loadLatestReading() {
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
        latestReading = data;
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
    setInterval(loadLatestReading, 10000); // Poll every 10 seconds
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
    // No need to add hours - created_at should already be in server timezone
    return date.toLocaleTimeString('en-US', { 
      hour: '2-digit', 
      minute: '2-digit',
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
    if (aqi <= 50) return 'green';
    if (aqi <= 100) return 'yellow';
    if (aqi <= 150) return 'orange';
    if (aqi <= 200) return 'red';
    return 'purple';
  }

  function getHealthMessage(aqi) {
    if (aqi <= 50) return 'Good air quality';
    if (aqi <= 100) return 'Moderate - sensitive groups should limit exertion';
    if (aqi <= 150) return 'Unhealthy for sensitive groups';
    if (aqi <= 200) return 'Unhealthy';
    return 'Very unhealthy';
  }
</script>

<main style="max-width: 1200px; margin: 0 auto; padding: 1rem;">
  <h1 style="font-size: 1.875rem; font-weight: bold; margin-bottom: 1.5rem;">Smart PM2.5 Dashboard</h1>
  <p style="margin-bottom: 1rem; font-size: 0.875rem; color: #666;">Backend URL: {BACKEND_BASE.toString()}</p>

  <div style="background: white; border-radius: 0.5rem; box-shadow: 0 1px 3px rgba(0,0,0,0.1); padding: 1.5rem; margin-bottom: 1.5rem;">
    <h2 class="text-xl font-semibold mb-4">Current Reading</h2>
    {#if loadingLatest}
      <p>Loading latest reading...</p>
    {:else if errorLatest}
      <p style="color: red;">Error: {errorLatest}</p>
      <div style="margin-top: 0.75rem; display:flex; gap:0.5rem; flex-wrap:wrap;">
        <button style="padding: 0.5rem 1rem; background: #10b981; color: white; border: none; border-radius: 0.25rem; cursor: pointer;" on:click={wakeBackend}>Wake backend</button>
        <button style="padding: 0.5rem 1rem; background: #3b82f6; color: white; border: none; border-radius: 0.25rem; cursor: pointer;" on:click={loadLatestReading}>Retry</button>
      </div>
    {:else if latestReading}
      <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 1rem;">
        <div style="text-align: center;">
          <div style="font-size: 1.5rem; font-weight: bold; color: {getAQIColor(latestReading.aqi)};">{latestReading.aqi}</div>
          <div style="font-size: 0.875rem; color: #666;">AQI</div>
        </div>
        <div style="text-align: center;">
          <div style="font-size: 1.5rem; font-weight: bold;">{latestReading.pm25}</div>
          <div style="font-size: 0.875rem; color: #666;">PM2.5 (µg/m³)</div>
        </div>
        <div style="text-align: center;">
          <div style="font-size: 1.5rem; font-weight: bold;">{latestReading.pm10}</div>
          <div style="font-size: 0.875rem; color: #666;">PM10 (µg/m³)</div>
        </div>
        <div style="text-align: center;">
          <div style="font-size: 1.5rem; font-weight: bold;">{latestReading.wifi_rssi}</div>
          <div style="font-size: 0.875rem; color: #666;">WiFi RSSI</div>
        </div>
      </div>
      <div style="margin-top: 1rem; padding: 0.75rem; background: #f0f0f0; border-radius: 0.25rem;">
        <strong>Health Recommendation:</strong> {getHealthMessage(latestReading.aqi)}
      </div>
    {:else}
      <p>No data available.</p>
    {/if}
  </div>

  <div style="background: white; border-radius: 0.5rem; box-shadow: 0 1px 3px rgba(0,0,0,0.1); padding: 1.5rem;">
    <h2 style="font-size: 1.25rem; font-weight: 600; margin-bottom: 1rem;">Historical Data</h2>
    <div style="margin-bottom: 1rem;">
      <button style="margin-right: 0.5rem; padding: 0.5rem 1rem; background: #10b981; color: white; border: none; border-radius: 0.25rem; cursor: pointer;" on:click={() => loadHistory('realtime')}>Live</button>
      <button style="margin-right: 0.5rem; padding: 0.5rem 1rem; background: #3b82f6; color: white; border: none; border-radius: 0.25rem; cursor: pointer;" on:click={() => loadHistory('5min')}>5min</button>
      <button style="margin-right: 0.5rem; padding: 0.5rem 1rem; background: #3b82f6; color: white; border: none; border-radius: 0.25rem; cursor: pointer;" on:click={() => loadHistory('30min')}>30min</button>
      <button style="margin-right: 0.5rem; padding: 0.5rem 1rem; background: #3b82f6; color: white; border: none; border-radius: 0.25rem; cursor: pointer;" on:click={() => loadHistory('1h')}>1h</button>
      <button style="margin-right: 0.5rem; padding: 0.5rem 1rem; background: #3b82f6; color: white; border: none; border-radius: 0.25rem; cursor: pointer;" on:click={() => loadHistory('4h')}>4h</button>
      <button style="padding: 0.5rem 1rem; background: #3b82f6; color: white; border: none; border-radius: 0.25rem; cursor: pointer;" on:click={() => loadHistory('24h')}>24h</button>
    </div>
    {#if loadingHistory}
      <p>Loading history...</p>
    {:else if errorHistory}
      <p style="color: red;">Error: {errorHistory}</p>
    {:else}
      <!-- Chart container with modern styling -->
      <div style="position: relative; height: 400px; width: 100%; background: linear-gradient(135deg, #f8fafc 0%, #f1f5f9 100%); border-radius: 12px; padding: 1rem; box-shadow: inset 0 1px 3px rgba(0,0,0,0.05); display: {historyData.length>0 ? 'block' : 'none'};">
        <canvas bind:this={chartCanvas} id="chart" style="width: 100%; height: 100%;"></canvas>
      </div>
      {#if historyData.length === 0}
        <p>No historical data available.</p>
      {/if}
    {/if}
  </div>
</main>