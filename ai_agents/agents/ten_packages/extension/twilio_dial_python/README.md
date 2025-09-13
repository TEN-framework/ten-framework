# Twilio Dial Extension

This extension provides dial call functionality using Twilio's Voice API. It currently supports outbound calls with custom messages through the TEN framework, with inbound call support planned for future releases.

## Features

- Make outbound calls to any phone number
- Send custom messages during calls
- Support for cmd_out commands to notify the system about call events
- Integration with TEN framework's LLM tool system
- Future: Inbound call handling and webhook support

## Configuration

The extension requires the following environment variables:

- `TWILIO_ACCOUNT_SID`: Your Twilio Account SID
- `TWILIO_AUTH_TOKEN`: Your Twilio Auth Token
- `TWILIO_FROM_NUMBER`: Your Twilio phone number (must be verified in Twilio console)

## Usage

The extension provides a tool called `make_outbound_call` that can be used by LLM agents:

```json
{
  "name": "make_outbound_call",
  "description": "Make an outbound call to a phone number with a message",
  "parameters": {
    "phone_number": "+1234567890",
    "message": "Hello, this is a test call from the AI assistant."
  }
}
```

## Commands

### cmd_out: make_call

When a call is initiated, the extension sends a `make_call` command with the following properties:

- `phone_number`: The destination phone number
- `message`: The message being spoken
- `call_sid`: Twilio's unique call identifier

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
