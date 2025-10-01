#!/usr/bin/env bash
# Vercel deployment script - updated for fixed configuration
# This script automates the Vercel deployment with proper environment variables

set -euo pipefail

PROJECT_NAME="smartpm25-frontend"
BUILD_COMMAND="npm run build"

echo "🚀 Smart PM2.5 Frontend - Vercel Deployment"
echo "============================================="

# Check if vercel CLI is available
if ! command -v vercel >/dev/null 2>&1; then
  echo "❌ Vercel CLI not found. Installing..."
  npm i -g vercel
fi

echo "📋 Deployment Configuration:"
echo "   Project: ${PROJECT_NAME}"
echo "   Root: web/frontend"
echo "   Build: ${BUILD_COMMAND}"
echo "   Output: build"
echo ""

read -p "🔍 Deploy to production? (y/N) " yn
# Convert to lowercase in a cross-shell compatible way
yn=$(echo "$yn" | tr '[:upper:]' '[:lower:]')
if [[ "$yn" != "y" ]]; then
  echo "❌ Deployment cancelled."
  exit 1
fi

echo "🔧 Deploying with environment variables..."

# Deploy with production environment variables
vercel --prod \
  --confirm \
  --name "${PROJECT_NAME}" \
  --build-env VITE_BACKEND_URL="https://smartpm2-5.onrender.com" \
  --build-env VITE_SUPABASE_URL="https://nbwcgngmnopimaapaeni.supabase.co" \
  --build-env VITE_SUPABASE_ANON_KEY="${VITE_SUPABASE_ANON_KEY:-}"

echo ""
echo "✅ Deployment completed!"
echo "🌐 Your app should be available at the URL shown above"
echo ""
echo "💡 Next steps:"
echo "   1. Visit your deployment URL to verify it works"
echo "   2. Check that real-time features are working"
echo "   3. Test on mobile devices"
echo "   4. Monitor for any errors in browser console"
echo ""
echo "🔧 If you encounter issues:"
echo "   1. Check environment variables in Vercel dashboard"
echo "   2. Verify backend CORS settings include your domain"
echo "   3. Check browser console for JavaScript errors"