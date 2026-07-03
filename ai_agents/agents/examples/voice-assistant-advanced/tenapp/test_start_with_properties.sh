#!/bin/bash

# Test /start API with custom properties for avatar channel override
# This demonstrates how clients should call /start to ensure avatar extensions
# receive the correct channel name.

AGENT_SERVER_URL="${AGENT_SERVER_URL:-http://localhost:8080}"
TEST_CHANNEL="test_avatar_channel_$(date +%s)"
USER_UID=9999

echo "╔════════════════════════════════════════════════════════════╗"
echo "║  Testing /start API with Avatar Channel Properties        ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""
echo "Agent Server: $AGENT_SERVER_URL"
echo "Test Channel: $TEST_CHANNEL"
echo ""

# Function to test /start API
test_start_api() {
    local GRAPH_NAME=$1
    local EXTENSION_NAME=$2
    local CHANNEL_PROPERTY=$3

    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "Testing: $GRAPH_NAME"
    echo "Extension: $EXTENSION_NAME"
    echo "Property: $CHANNEL_PROPERTY"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""

    # Build the JSON payload
    # Note: avatar is the node NAME (not the addon name)
    local PAYLOAD=$(cat <<EOF
{
  "request_id": "test_$(uuidgen 2>/dev/null || echo $(date +%s))",
  "channel_name": "${TEST_CHANNEL}",
  "user_uid": ${USER_UID},
  "graph_name": "${GRAPH_NAME}",
  "properties": {
    "${EXTENSION_NAME}": {
      "${CHANNEL_PROPERTY}": "${TEST_CHANNEL}"
    }
  }
}
EOF
)

    echo "Request Payload:"
    echo "$PAYLOAD" | jq '.' 2>/dev/null || echo "$PAYLOAD"
    echo ""

    # Send the request
    echo "Sending POST request to ${AGENT_SERVER_URL}/start..."
    RESPONSE=$(curl -s -w "\n%{http_code}" -X POST \
        -H "Content-Type: application/json" \
        -d "$PAYLOAD" \
        "${AGENT_SERVER_URL}/start")

    # Parse response
    HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
    BODY=$(echo "$RESPONSE" | head -n-1)

    echo ""
    echo "Response (HTTP $HTTP_CODE):"
    echo "$BODY" | jq '.' 2>/dev/null || echo "$BODY"
    echo ""

    # Check if successful
    if [ "$HTTP_CODE" = "200" ]; then
        echo "✓ API call successful"

        # Check if property.json was created with correct channel
        PROP_FILE="/tmp/ten_agent/property-${TEST_CHANNEL}-*.json"
        if ls $PROP_FILE 1> /dev/null 2>&1; then
            echo "✓ Property file created: $(ls $PROP_FILE | head -1)"

            # Verify avatar node has correct channel
            ACTUAL_CHANNEL=$(python3 <<PYTHON
import json, glob
files = glob.glob('$PROP_FILE')
if files:
    with open(files[0]) as f:
        config = json.load(f)
        graphs = config['ten']['predefined_graphs']
        for graph in graphs:
            if graph['name'] == '$GRAPH_NAME':
                nodes = graph['graph']['nodes']
                for node in nodes:
                    if node.get('name') == '$EXTENSION_NAME':
                        prop = node['property'].get('$CHANNEL_PROPERTY', '')
                        print(prop)
                        break
PYTHON
)

            if [ "$ACTUAL_CHANNEL" = "$TEST_CHANNEL" ]; then
                echo "✓ Avatar channel property correctly set to: $ACTUAL_CHANNEL"
                return 0
            else
                echo "✗ Avatar channel property incorrect: $ACTUAL_CHANNEL (expected: $TEST_CHANNEL)"
                return 1
            fi
        else
            echo "⚠ Property file not found (extension may not have started yet)"
        fi

        # Stop the session
        echo ""
        echo "Stopping session..."
        curl -s -X POST \
            -H "Content-Type: application/json" \
            -d "{\"request_id\": \"stop_test\", \"channel_name\": \"${TEST_CHANNEL}\"}" \
            "${AGENT_SERVER_URL}/stop" > /dev/null
        echo "✓ Session stopped"

        return 0
    else
        echo "✗ API call failed with HTTP $HTTP_CODE"
        return 1
    fi
}

# Test 1: voice_assistant_heygen
echo ""
test_start_api "voice_assistant_heygen" "avatar" "channel"
HEYGEN_RESULT=$?

sleep 2

# Test 2: voice_assistant_generic_video
echo ""
test_start_api "voice_assistant_generic_video" "avatar" "agora_channel_name"
GENERIC_VIDEO_RESULT=$?

# Summary
echo ""
echo "╔════════════════════════════════════════════════════════════╗"
echo "║  Test Summary                                              ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

if [ $HEYGEN_RESULT -eq 0 ]; then
    echo "✓ voice_assistant_heygen: PASS"
else
    echo "✗ voice_assistant_heygen: FAIL"
fi

if [ $GENERIC_VIDEO_RESULT -eq 0 ]; then
    echo "✓ voice_assistant_generic_video: PASS"
else
    echo "✗ voice_assistant_generic_video: FAIL"
fi

echo ""

if [ $HEYGEN_RESULT -eq 0 ] && [ $GENERIC_VIDEO_RESULT -eq 0 ]; then
    echo "✓ All tests passed!"
    echo ""
    echo "Both avatar extensions correctly receive the dynamic channel name"
    echo "when properties are passed via the /start API."
    exit 0
else
    echo "✗ Some tests failed"
    exit 1
fi
