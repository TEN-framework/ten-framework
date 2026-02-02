#!/bin/bash
# Example: How to run proxy server in a container with openai_tts

# Start proxy server in background
echo "Starting OpenAI TTS Proxy on port 8081..."
python /app/proxy/proxy_server.py &
PROXY_PID=$!

# Wait for proxy to be ready
sleep 2

# Check if proxy is running
if ! kill -0 $PROXY_PID 2>/dev/null; then
    echo "Error: Proxy server failed to start"
    exit 1
fi

echo "Proxy server started (PID: $PROXY_PID)"

# Your main application would start here
# For example, starting a TEN agent that uses openai_tts2_python
# with base_url configured to http://localhost:8081

# Wait for proxy process
wait $PROXY_PID
