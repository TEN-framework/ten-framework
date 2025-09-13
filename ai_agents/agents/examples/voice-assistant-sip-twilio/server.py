#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import json
import os
from typing import Dict, Any
from fastapi import FastAPI, HTTPException, Request, WebSocket
from fastapi.responses import JSONResponse
import uvicorn

# Twilio imports
from twilio.rest import Client
from twilio.twiml.voice_response import VoiceResponse

app = FastAPI(title="Twilio Dial Server", version="1.0.0")

# Global variables for active call sessions
active_call_sessions: Dict[str, Dict[str, Any]] = {}


@app.post("/start_call")
async def start_call(request: Request):
    """Start a new call session with Twilio"""
    try:
        # Parse request body
        body = await request.json()
        phone_number = body.get("phone_number")
        message = body.get("message", "Hello, this is a call from the AI assistant.")

        if not phone_number:
            raise HTTPException(status_code=400, detail="phone_number is required")

        # Get Twilio credentials from environment
        account_sid = os.getenv("TWILIO_ACCOUNT_SID")
        auth_token = os.getenv("TWILIO_AUTH_TOKEN")
        from_number = os.getenv("TWILIO_FROM_NUMBER")
        webhook_url = os.getenv("TWILIO_WEBHOOK_URL")

        if not all([account_sid, auth_token, from_number, webhook_url]):
            raise HTTPException(
                status_code=500,
                detail="Twilio credentials or webhook URL not configured"
            )

        # Create Twilio client
        client = Client(account_sid, auth_token)

        # Create TwiML response with WebSocket streaming
        twiml_response = VoiceResponse()
        twiml_response.say(message, voice='alice')
        twiml_response.start().stream(url=f"wss://{webhook_url}/ws")
        twiml_response.pause(length=30)

        # Make the call
        call = client.calls.create(
            to=phone_number,
            from_=from_number,
            twiml=str(twiml_response)
        )

        # Store call session
        active_call_sessions[call.sid] = {
            "phone_number": phone_number,
            "message": message,
            "status": call.status,
            "websocket": None
        }

        return JSONResponse(content={
            "success": True,
            "call_sid": call.sid,
            "phone_number": phone_number,
            "message": message,
            "status": call.status
        })

    except Exception as e:
        return JSONResponse(
            status_code=500,
            content={"success": False, "error": str(e)}
        )


@app.post("/stop_call")
async def stop_call(request: Request):
    """Stop a call session"""
    try:
        body = await request.json()
        call_sid = body.get("call_sid")

        if not call_sid:
            raise HTTPException(status_code=400, detail="call_sid is required")

        if call_sid not in active_call_sessions:
            raise HTTPException(status_code=404, detail="Call session not found")

        # Get Twilio credentials
        account_sid = os.getenv("TWILIO_ACCOUNT_SID")
        auth_token = os.getenv("TWILIO_AUTH_TOKEN")

        if account_sid and auth_token:
            # Hang up the call via Twilio API
            client = Client(account_sid, auth_token)
            call = client.calls(call_sid).update(status='completed')

        # Remove from active sessions
        del active_call_sessions[call_sid]

        return JSONResponse(content={
            "success": True,
            "message": f"Call {call_sid} stopped"
        })

    except Exception as e:
        return JSONResponse(
            status_code=500,
            content={"success": False, "error": str(e)}
        )


@app.get("/health")
async def health_check():
    """Health check endpoint"""
    return JSONResponse(content={
        "status": "healthy",
        "active_calls": len(active_call_sessions)
    })


@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    """WebSocket endpoint for Twilio media streaming"""
    await websocket.accept()
    print("WebSocket connection established with Twilio")

    try:
        # Receive start event
        start_data = await websocket.receive_json()
        stream_sid = start_data["start"]["streamSid"]
        call_sid = start_data["start"]["callSid"]
        print(f"Received start event for streamSid: {stream_sid}, callSid: {call_sid}")

        # Store WebSocket connection in active session
        if call_sid in active_call_sessions:
            active_call_sessions[call_sid]["websocket"] = websocket
        else:
            print(f"Warning: Call SID {call_sid} not found in active sessions.")

        # Process incoming messages
        while True:
            message = await websocket.receive_json()

            if message["event"] == "media":
                # Handle incoming audio data
                payload = message["media"]["payload"]
                print(f"Received audio data: {len(payload)} bytes")
                # Here you would forward the audio to your TEN agent

            elif message["event"] == "stop":
                print(f"Received stop event for stream {stream_sid}")
                break

            elif message["event"] == "mark":
                print(f"Received mark event for stream {stream_sid}: {message['mark']['name']}")

            else:
                print(f"Received unknown event: {message['event']}")

    except Exception as e:
        print(f"WebSocket error: {e}")
    finally:
        print(f"WebSocket connection closed for stream {stream_sid}")
        # Clean up WebSocket reference
        for call_sid, session in active_call_sessions.items():
            if session.get("websocket") == websocket:
                session["websocket"] = None
                break


if __name__ == "__main__":
    # Run the FastAPI server
    uvicorn.run(
        "server:app",
        host="0.0.0.0",
        port=8000,
        reload=True
    )
