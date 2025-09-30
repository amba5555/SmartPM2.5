<script>
  import { onMount } from 'svelte';
  import { supabase } from '$lib/supabase';
  import Chart from 'chart.js/auto';

  let latestReading = null;
  let historyData = [];
  let chart;

  onMount(async () => {
    await loadLatestReading();
    await loadHistory();
    setupPolling();
  });

  async function loadLatestReading() {
    const { data, error } = await supabase
      .from('readings')
      .select('*')
      .order('timestamp', { ascending: false })
      .limit(1);

    if (error) {
      console.error('Error loading latest reading:', error);
    } else {
      latestReading = data[0];
    }
  }

  async function loadHistory(period = '24h') {
    const now = Date.now();
    let startTime;
    if (period === '7d') startTime = now - 7 * 24 * 60 * 60 * 1000;
    else if (period === '30d') startTime = now - 30 * 24 * 60 * 60 * 1000;
    else startTime = now - 24 * 60 * 60 * 1000;

    const { data, error } = await supabase
      .from('readings')
      .select('*')
      .gte('timestamp', startTime)
      .order('timestamp');

    if (error) {
      console.error('Error loading history:', error);
    } else {
      historyData = data;
      updateChart();
    }
  }

  function setupPolling() {
    setInterval(loadLatestReading, 10000); // Poll every 10 seconds
  }

  function updateChart() {
    const ctx = document.getElementById('chart').getContext('2d');
    if (chart) chart.destroy();

    chart = new Chart(ctx, {
      type: 'line',
      data: {
        labels: historyData.map(d => new Date(d.timestamp).toLocaleTimeString()),
        datasets: [{
          label: 'PM2.5',
          data: historyData.map(d => d.pm25),
          borderColor: 'rgb(75, 192, 192)',
          tension: 0.1
        }]
      }
    });
  }

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

<main class="container mx-auto p-4">
  <h1 class="text-3xl font-bold mb-6">Smart PM2.5 Dashboard</h1>

  {#if latestReading}
    <div class="bg-white rounded-lg shadow-md p-6 mb-6">
      <h2 class="text-xl font-semibold mb-4">Current Reading</h2>
      <div class="grid grid-cols-2 md:grid-cols-4 gap-4">
        <div class="text-center">
          <div class="text-2xl font-bold text-{getAQIColor(latestReading.aqi)}">{latestReading.aqi}</div>
          <div class="text-sm text-gray-600">AQI</div>
        </div>
        <div class="text-center">
          <div class="text-2xl font-bold">{latestReading.pm25}</div>
          <div class="text-sm text-gray-600">PM2.5 (µg/m³)</div>
        </div>
        <div class="text-center">
          <div class="text-2xl font-bold">{latestReading.pm10}</div>
          <div class="text-sm text-gray-600">PM10 (µg/m³)</div>
        </div>
        <div class="text-center">
          <div class="text-2xl font-bold">{latestReading.wifi_rssi}</div>
          <div class="text-sm text-gray-600">WiFi RSSI</div>
        </div>
      </div>
      <div class="mt-4 p-3 bg-gray-100 rounded">
        <strong>Health Recommendation:</strong> {getHealthMessage(latestReading.aqi)}
      </div>
    </div>
  {/if}

  <div class="bg-white rounded-lg shadow-md p-6">
    <h2 class="text-xl font-semibold mb-4">Historical Data</h2>
    <div class="mb-4">
      <button class="mr-2 px-4 py-2 bg-blue-500 text-white rounded" on:click={() => loadHistory('24h')}>24h</button>
      <button class="mr-2 px-4 py-2 bg-blue-500 text-white rounded" on:click={() => loadHistory('7d')}>7d</button>
      <button class="px-4 py-2 bg-blue-500 text-white rounded" on:click={() => loadHistory('30d')}>30d</button>
    </div>
    <canvas id="chart" width="400" height="200"></canvas>
  </div>
</main>

<style>
  .container {
    max-width: 1200px;
  }
</style>