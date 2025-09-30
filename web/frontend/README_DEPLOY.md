# Frontend Deployment (Vercel / Netlify / Render)

This document describes how to deploy the `web/frontend` SvelteKit app as a static site. The project is configured to use `@sveltejs/adapter-static` and produces a `build` directory which can be hosted on Vercel, Netlify, or Render's static site feature.

## Environment variables

Create a `.env.production` (or set environment variables in your hosting provider) with at least the following values:

- `VITE_BACKEND_URL` - URL of the backend API (e.g. `https://smartpm2-5.onrender.com`)
- `VITE_SUPABASE_URL` - Your Supabase project URL (for realtime features)
- `VITE_SUPABASE_ANON_KEY` - Public anon key from Supabase

> On Vercel/Netlify, prefix variables exactly as above (VITE_...) to make them available at build time.

## Vercel (recommended)

1. Connect your GitHub repository to Vercel.
2. In the project settings, set the Root Directory to `web/frontend`.
3. Set the Build Command to `npm run vercel-build` and the Output Directory to `build`.
4. Add the required environment variables in Vercel's dashboard.
5. Deploy. The site will be served as a static site and `index.html` will be used for client-side routing.

## Netlify

1. Create a new site from Git.
2. Set the base directory to `web/frontend`.
3. Build Command: `npm run build`
4. Publish directory: `build`
5. Add environment variables in Netlify's dashboard.

## Render Static Site

1. Create a new Static Site in Render.
2. Connect your GitHub repo and set the Root Directory to `web/frontend`.
3. Build Command: `npm run build`
4. Publish Directory: `build`
5. Add environment variables in Render's static site settings.

## Local Build & Preview

```bash
cd web/frontend
npm install
npm run build
npx serve build   # or use any static file server
```

## Quick Vercel CLI instructions

If you prefer to use the Vercel CLI instead of the dashboard, install and login:

```bash
npm i -g vercel
vercel login
```

From the repo root you can run the helper script which will call the CLI and deploy the project. The script will prompt you for confirmation and will not store secrets in the repo:

```bash
cd web/frontend
./vercel-deploy.sh
```

If you prefer manual CLI commands, here are the essential ones:

```bash
# from repo root
cd web/frontend
vercel --prod --confirm --name smartpm25-frontend --build-env VITE_BACKEND_URL --build-env VITE_SUPABASE_URL --build-env VITE_SUPABASE_ANON_KEY
```

The CLI will guide you interactively through linking or creating the Vercel project and setting environment variables if you supply them interactively or via `vercel env add`.

## Notes
- The app uses client-side routing. The `vercel.json` included configures fallback routing to `index.html`.
- If you rely on Supabase realtime features in production, ensure `VITE_SUPABASE_URL` and `VITE_SUPABASE_ANON_KEY` are set in the hosting provider.
- For dynamic server-side features (edge functions, server API routes), deploy the backend separately (see `web/backend/README.md`).
