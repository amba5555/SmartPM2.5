// Defensive Supabase client factory.
// Returns a real Supabase client when VITE_SUPABASE_URL and VITE_SUPABASE_ANON_KEY
// are present at build time. Otherwise returns a minimal stub that implements
// channel(...) and removeChannel(...) so the app won't throw during startup.
import { createClient } from '@supabase/supabase-js';

const url = import.meta.env.VITE_SUPABASE_URL || '';
const key = import.meta.env.VITE_SUPABASE_ANON_KEY || '';

function makeStub() {
  const noop = () => ({
    on: () => ({ subscribe: () => ({}) }),
    subscribe: () => ({}),
    unsubscribe: () => ({}),
  });

  return {
    channel: () => ({
      on: () => ({ subscribe: () => ({}) }),
      subscribe: () => ({ subscribe: () => ({}) }),
    }),
    removeChannel: () => {},
  };
}

export default function getSupabaseClient() {
  try {
    if (url && key) {
      return createClient(url, key);
    }
  } catch (e) {
    // swallow and fall through to stub
    console.warn('Failed to create Supabase client:', e?.message || e);
  }
  console.warn('Supabase env vars missing at build time; using stub client.');
  return makeStub();
}
