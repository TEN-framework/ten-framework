#!/bin/bash

# Voice Assistant Frontend Startup Script

echo "🚀 Starting Voice Assistant Frontend..."

# Check if node_modules exists
if [ ! -d "node_modules" ]; then
    echo "📦 Installing dependencies..."
    npm install --verbose
fi

npm run dev
