# Live2D Voice Assistant Frontend

A modern, clean frontend for the Live2D Voice Assistant with real-time voice communication and character interaction.

## Features

- 🎭 **Live2D Character Support** - Interactive Live2D models with audio synchronization
- 🎤 **Real-time Voice Communication** - Powered by Agora RTC
- 💬 **Live Transcript** - Real-time conversation transcript via Agora RTM
- 🤖 **Agent Management** - Start/stop/ping agent process controls
- 📱 **Responsive Design** - Works on desktop and mobile devices
- 🎨 **Modern UI** - Clean, intuitive interface with dark mode support

## Quick Start

1. **Install dependencies:**
   ```bash
   cd frontend2
   npm install
   # or
   pnpm install
   ```

2. **Configure environment:**
   ```bash
   cp env.example .env.local
   # Edit .env.local with your Agora App ID
   ```

3. **Run the development server:**
   ```bash
   npm run dev
   # or
   pnpm dev
   ```

4. **Access the application:**
   - Frontend: http://localhost:3000

## Configuration

### Agora Setup

1. Get your Agora App ID from [Agora Console](https://console.agora.io/)
2. Add it to your `.env.local` file:
   ```
   NEXT_PUBLIC_AGORA_APP_ID=your_agora_app_id_here
   ```

### Live2D Models

The application includes two pre-configured Live2D models:
- **Hiyori** - Japanese character with multiple animations
- **Kei** - Multi-language character with voice sync

Models are located in `public/models/` and can be easily swapped or extended.

## Architecture

### Core Services

- **AgoraService** (`src/services/agora.ts`) - Handles RTC/RTM communication
- **AgentService** (`src/services/agent.ts`) - Manages agent process lifecycle

### Key Components

- **Live2DCharacter** - Renders and animates Live2D models with audio sync
- **ConnectionPanel** - Manages Agora connection and agent controls
- **TranscriptPanel** - Displays real-time conversation transcript

### Tech Stack

- **Next.js 15** - React framework with App Router
- **TypeScript** - Type-safe development
- **Tailwind CSS** - Utility-first styling
- **Radix UI** - Accessible component primitives
- **PIXI.js** - 2D rendering for Live2D
- **Agora RTC/RTM** - Real-time communication

## Development

### Project Structure

```
src/
├── app/                 # Next.js App Router
├── components/          # React components
│   ├── ui/             # Reusable UI components
│   ├── Live2DCharacter.tsx
│   ├── ConnectionPanel.tsx
│   └── TranscriptPanel.tsx
├── services/           # Business logic
│   ├── agora.ts       # Agora RTC/RTM service
│   └── agent.ts       # Agent management service
├── types/             # TypeScript type definitions
└── lib/               # Utility functions
```

### Available Scripts

- `npm run dev` - Start development server
- `npm run build` - Build for production
- `npm run start` - Start production server
- `npm run lint` - Run ESLint

## Features in Detail

### Live2D Integration

- Automatic model loading and initialization
- Audio-driven mouth movement synchronization
- Idle and talking animations
- Responsive scaling and positioning

### Real-time Communication

- Agora RTC for audio streaming
- Agora RTM for transcript data
- Network quality monitoring
- Connection status indicators

### Agent Management

- Start/stop agent processes
- Health monitoring with ping
- Error handling and status reporting
- RESTful API integration

### Transcript System

- Real-time message display
- User/assistant message differentiation
- Confidence score visualization
- Export functionality
- Auto-scroll and manual controls

## Browser Support

- Chrome 88+
- Firefox 85+
- Safari 14+
- Edge 88+

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## License

This project is part of the Ten Agent examples and follows the same licensing terms.
