# Twilio Dial Extension

This extension provides real-time voice call functionality using Twilio's Voice API with WebSocket streaming. It establishes WebSocket connections for real-time audio streaming and integrates with the TEN framework for ASR/TTS processing.

## Features

- Real-time bidirectional audio streaming via WebSocket
- Make outbound calls to any phone number
- Support for cmd_in commands to receive call requests
- Return call results via CmdResult
- Integration with TEN framework for ASR/TTS processing
- WebSocket server for Twilio audio streaming

## Configuration

The extension requires the following environment variables:

- `TWILIO_ACCOUNT_SID`: Your Twilio Account SID
- `TWILIO_AUTH_TOKEN`: Your Twilio Auth Token
- `TWILIO_FROM_NUMBER`: Your Twilio phone number (must be verified in Twilio console)
- `TWILIO_WEBHOOK_URL`: Your public webhook URL for Twilio to connect to

## Usage

The extension receives `make_call` commands via cmd_in and returns results via CmdResult:

#### cmd_in: make_call
Send a command to initiate an outbound call:
```json
{
  "name": "make_call",
  "phone_number": "+1234567890",
  "message": "Hello, this is a test call from the AI assistant."
}
```

## Commands

### cmd_in: make_call

The extension processes `make_call` commands and returns results via CmdResult:

**Success Response:**
- Status: OK
- Properties:
  - `call_sid`: Twilio's unique call identifier
  - `phone_number`: The destination phone number
  - `status`: Call status from Twilio

**Error Response:**
- Status: ERROR
- Message: Error description

## Requirements

- Python 3.8+
- Twilio account with Voice API access
- Verified phone number in Twilio console

## Installation

1. Install dependencies: `pip install -r requirements.txt`
2. Set up environment variables
3. Configure the extension in your TEN framework setup

## License

Licensed under the Apache License, Version 2.0.
