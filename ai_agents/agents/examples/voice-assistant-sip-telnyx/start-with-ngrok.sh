#!/bin/bash

# Start the voice assistant with ngrok for local development

# Check if ngrok is installed
if ! command -v ngrok &> /dev/null; then
    echo "ngrok not found. Installing..."
    curl -s https://ngrok-agent.s3.amazonaws.com/ngrok.asc | gpg --dearmor > /tmp/ngrok.gpg
    sudo mv /tmp/ngrok.gpg /etc/apt/trusted.gpg.d/ngrok.gpg
    echo "deb https://ngrok-agent.s3.amazonaws.com buster main" | sudo tee /etc/apt/sources.list.d/ngrok.list
    sudo apt-get update
    sudo apt-get install -y ngrok
fi

# Check if authtoken is configured
if ! grep -q "authtoken" ~/.ngrok2/ngrok.yml 2>/dev/null; then
    if [ -z "$NGROK_AUTHTOKEN" ]; then
        echo "Error: NGROK_AUTHTOKEN environment variable is not set."
        echo "Please set NGROK_AUTHTOKEN with your ngrok auth token."
        echo "You can get it from: https://dashboard.ngrok.com/get-started/your-authtoken"
        exit 1
    fi
    ngrok authtoken $NGROK_AUTHTOKEN
fi

echo "Starting ngrok tunnel..."
ngrok http 9000 --log=stdout &
NGROK_PID=$!

# Wait for ngrok to start
sleep 3

# Get the public URL
NGROK_URL=$(curl -s http://localhost:4040/api/tunnels | python3 -c "import sys, json; print(json.load(sys.stdin)['tunnels'][0]['public_url'])" 2>/dev/null)

if [ -z "$NGROK_URL" ]; then
    echo "Error: Could not get ngrok URL. Check ngrok logs."
    kill $NGROK_PID 2>/dev/null
    exit 1
fi

echo "ngrok tunnel started: $NGROK_URL"
echo ""
echo "Configure your .env file with:"
echo "  TELNYX_PUBLIC_SERVER_URL=$(echo $NGROK_URL | sed 's/http:\/\///' | sed 's/https:\/\///')"
echo ""
echo "Make sure to configure your Telnyx webhook to point to:"
echo "  $NGROK_URL/webhook/status"
echo ""
echo "Press Ctrl+C to stop the tunnel and the application."

# Run the voice assistant
cd "$(dirname $0)"
python server/main.py --tenapp-dir tenapp --port 9000 &

# Wait for user input
wait