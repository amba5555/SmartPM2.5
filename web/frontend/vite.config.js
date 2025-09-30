import { sveltekit } from '@sveltejs/kit/vite';
import { defineConfig } from 'vite';

export default defineConfig({
  plugins: [sveltekit()],
  define: {
    // Fix potential issues with environment variables
    global: 'globalThis',
  },
  build: {
    // Increase chunk size warning limit
    chunkSizeWarningLimit: 1000
  },
  optimizeDeps: {
    include: ['chart.js', '@supabase/supabase-js']
  },
  server: {
    fs: {
      allow: ['..']
    }
  }
});