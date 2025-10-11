#!/bin/bash

# Setup script for Live2D Voice Assistant Frontend

echo "🎭 Setting up Live2D Voice Assistant Frontend..."

# Check if Node.js is installed
if ! command -v node &> /dev/null; then
    echo "❌ Node.js is not installed. Please install Node.js 20+ first."
    exit 1
fi

# Check Node.js version
NODE_VERSION=$(node -v | cut -d'v' -f2 | cut -d'.' -f1)
if [ "$NODE_VERSION" -lt 20 ]; then
    echo "❌ Node.js version 20+ is required. Current version: $(node -v)"
    exit 1
fi

echo "✅ Node.js version: $(node -v)"

# Check if pnpm is installed
if ! command -v pnpm &> /dev/null; then
    echo "📦 Installing pnpm..."
    npm install -g pnpm
fi

echo "✅ pnpm version: $(pnpm -v)"

# Install dependencies
echo "📦 Installing dependencies..."
pnpm install

# Create .env.local if it doesn't exist
if [ ! -f .env.local ]; then
    echo "⚙️  Creating .env.local from template..."
    cp env.example .env.local
    echo "📝 Please edit .env.local and add your Agora App ID"
fi

# Check if Live2D models exist
if [ ! -d "public/models" ]; then
    echo "⚠️  Live2D models not found. Please ensure models are in public/models/"
fi

echo "🎉 Setup complete!"
echo ""
echo "Next steps:"
echo "1. Edit .env.local and add your Agora App ID"
echo "2. Run 'pnpm dev' to start the development server"
echo "3. Open http://localhost:3000 in your browser"
echo ""
echo "For more information, see README.md"
