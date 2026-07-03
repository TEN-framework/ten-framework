#!/bin/bash

# Twilio Voice Assistant Frontend Startup Script

echo "🚀 Starting Twilio Voice Assistant Frontend..."

# Check if node_modules exists
if [ ! -d "node_modules" ]; then
    echo "📦 Installing dependencies..."
    npm install
fi

# Check if .env.local exists
if [ ! -f ".env.local" ]; then
    echo "⚙️  Creating .env.local file..."
    cat > .env.local << EOF
# Twilio Server Configuration
NEXT_PUBLIC_TWILIO_SERVER_URL=http://localhost:8080
EOF
    echo "✅ Created .env.local file with default configuration"
    echo "📝 You can modify NEXT_PUBLIC_TWILIO_SERVER_URL in .env.local if needed"
fi

echo "🎯 Starting development server..."
echo "📱 Frontend will be available at: http://localhost:3000"
echo "🔗 Make sure your Twilio server is running on: http://localhost:8080"
echo ""

npm run dev
