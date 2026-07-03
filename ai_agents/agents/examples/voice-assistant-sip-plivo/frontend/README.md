# Twilio Voice Assistant Frontend

This is a Next.js frontend application for interacting with the Twilio voice assistant backend, supporting both outbound and inbound call functionality.

## Features

- **Outbound Calls**: Enter phone number and message to initiate outbound calls
- **Inbound Calls**: Use popup dialog to enter phone number and initiate inbound calls
- **Real-time Status**: Display call status and WebSocket connection status
- **Call Management**: End ongoing calls

## Installation and Setup

### 1. Install Dependencies

```bash
cd frontend
npm install
```

### 2. Configure Environment Variables

Create `.env.local` file:

```bash
# Twilio Server Configuration
NEXT_PUBLIC_TWILIO_SERVER_URL=http://localhost:8080
```

Or copy the example file:
```bash
cp .env.example .env.local
```

### 3. Start Development Server

```bash
npm run dev
```

The application will run at http://localhost:3000.

## Project Structure

```
frontend/
├── app/
│   ├── globals.css          # Global styles
│   ├── layout.tsx           # Root layout
│   └── page.tsx             # Main page
├── components/
│   ├── CallStatus.tsx       # Call status component
│   ├── InboundCallModal.tsx # Inbound call modal
│   └── OutboundCallForm.tsx # Outbound call form
├── lib/
│   └── api.ts               # API service
├── package.json
├── tailwind.config.js       # Tailwind configuration
├── tsconfig.json            # TypeScript configuration
└── next.config.js           # Next.js configuration
```

## API Endpoints

The frontend communicates with the backend through the following APIs:

- `POST /api/call` - Create a call (tenapp server)
- `GET /api/call/{call_sid}` - Get call information (tenapp server)
- `DELETE /api/call/{call_sid}` - End a call (tenapp server)
- `GET /api/calls` - List all calls (tenapp server)
- `GET /api/config` - Get server configuration (twilio server)
- `GET /health` - Health check

## Tech Stack

- **Next.js 14** - React framework
- **TypeScript** - Type safety
- **Tailwind CSS** - Styling framework
- **Axios** - HTTP client
- **Lucide React** - Icon library

## Usage Instructions

### Outbound Calls
1. Enter phone number in the "Outbound Calls" section
2. Optional: Modify the default message
3. Click "Initiate Outbound Call" button

### Inbound Calls
1. Click "Initiate Inbound Call" button
2. Enter phone number in the popup dialog
3. Click "Initiate Call"

### Call Management
- After a call starts, call status will be displayed at the bottom of the page
- You can view real-time call status, WebSocket connection status, etc.
- During an active call, you can click "End Call" button to terminate the call
