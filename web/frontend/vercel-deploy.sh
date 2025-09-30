#!/usr/bin/env bash
# vercel-deploy.sh
# Small helper to create + deploy the frontend to Vercel using the Vercel CLI.
# Usage: run this script after installing the Vercel CLI and logging in (`vercel login`).

set -euo pipefail

# Configurable defaults
PROJECT_NAME="smartpm25-frontend"
ROOT_DIR="web/frontend"
BUILD_COMMAND="npm run vercel-build"
OUTPUT_DIR="build"

echo "This script will create or link a Vercel project and deploy the static frontend."
read -p "Proceed? (y/N) " yn
if [[ "${yn,,}" != "y" ]]; then
  echo "Cancelled."
  exit 1
fi

# Ensure vercel is available
if ! command -v vercel >/dev/null 2>&1; then
  echo "Vercel CLI not found. Install with: npm i -g vercel" >&2
  exit 2
fi

# Create or link project
echo "Creating/linking project '${PROJECT_NAME}' with root '${ROOT_DIR}'..."
vercel project add ${PROJECT_NAME} || true

# Deploy using the CLI (this will prompt if project not yet linked)
cd "${ROOT_DIR}"

echo "Running vercel --prod to deploy (you may be prompted to configure)..."
vercel --prod --confirm --name "${PROJECT_NAME}" --build-env VITE_BACKEND_URL --build-env VITE_SUPABASE_URL --build-env VITE_SUPABASE_ANON_KEY

cat <<EOF
Deployment complete (or started). Visit the Vercel dashboard to set environment variables if they weren't provided interactively.
Note: This script does NOT save secrets to the repository. Use Vercel Dashboard or 'vercel env add' to set values securely.
EOF
