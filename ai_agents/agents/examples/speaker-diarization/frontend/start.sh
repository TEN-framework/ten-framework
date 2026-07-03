#!/bin/bash

# Speechmatics Diarization Frontend Startup Script

echo "🚀 Starting Speechmatics Diarization Frontend..."

# Check if node_modules exists
if [ ! -d "node_modules" ]; then
    echo "📦 Installing dependencies..."
    npm install --verbose
fi

npm run dev
