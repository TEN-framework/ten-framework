# Voice Assistant Companion - Phone Call Interface

## 🎨 Design Overview

The Voice Assistant Companion features a unique **phone call interface** inspired by modern smartphone calling apps, with Live2D character integration for a more engaging and personal experience.

## ✨ Key Features

### 1. **Phone Call Aesthetics**
- **Circular Live2D character display** - mimics a contact photo in a phone call
- **Call duration timer** - displays the active call time
- **Call/End call buttons** - green to call, red to hang up
- **Microphone mute toggle** - accessible during active calls
- **Gradient purple/indigo/blue background** - modern and calming

### 2. **Visual Feedback**
- **Pulsing ring animation** when connected
- **Green ring glow** when AI is speaking
- **Animated background gradients** for depth
- **Floating circle animations** for atmosphere
- **Status indicators** (Connected, Muted)

### 3. **Call States**

#### Idle State (Not Connected)
- Shows companion avatar in circular frame
- "Tap the call button to start" message
- Green call button (phone icon)
- Minimal UI for clean appearance

#### Calling State (Connecting)
- "Connecting..." status message
- Spinning loader animation
- Gray button with loading indicator
- Cannot interact until connected

#### Active Call State (Connected)
- "Call in Progress" header
- Live call duration timer (MM:SS format)
- Microphone mute/unmute button
- Red end call button (hang up icon)
- Status badges (Connected, Muted if applicable)
- Live2D character responds to audio
- Green glow when AI companion is speaking

## 🎭 Live2D Integration

### Character Display
- **Circular frame** (phone contact style)
- **Responsive animations** based on audio input
- **Lip-sync support** via MotionSync
- **Heart emissions** when speaking (optional effect)
- **Aspect ratio preserved** within circular container

### Speaking Indicators
1. **Ring glow** - green ring around character when speaking
2. **Pulse animation** - subtle breathing effect
3. **Heart emitter** - hearts float up from character
4. **Status text** - real-time feedback

## 🎨 Color Scheme

### Background
```css
Primary: Purple-900 to Blue-900 gradient
Accent layers:
  - Purple-500/20 (pulsing)
  - Blue-500/20 (pulsing, delayed)
  - Indigo-500/20 (pulsing, delayed)
```

### Interactive Elements
```css
Call button (idle): Green-500
End call button: Red-500
Mic button: White/20 (unmuted), Gray-700 (muted)
Status rings: Green-400 (active), White/20 (inactive)
```

### Typography
```css
Title: Inter font, 600-700 weight
Body: Poppins font, 400-600 weight
```

## 📱 Layout Structure

```
┌─────────────────────────────────┐
│       "AI Companion"            │  ← Header
│        (00:45 if active)        │  ← Call timer
├─────────────────────────────────┤
│                                 │
│    ┌─────────────────┐         │
│    │                 │         │
│    │   Live2D        │         │  ← Character circle
│    │   Character     │         │
│    │   (circular)    │         │
│    │                 │         │
│    └─────────────────┘         │
│                                 │
├─────────────────────────────────┤
│   "Hi! I'm here to help..."    │  ← Status message
├─────────────────────────────────┤
│                                 │
│    [🎤]  [📞]  [  ]            │  ← Controls
│   (Mic) (Call) (Space)         │
│                                 │
├─────────────────────────────────┤
│  ● Connected  ● Muted          │  ← Status badges
└─────────────────────────────────┘
```

## 🔧 Technical Implementation

### Components Used
1. **ClientOnlyLive2D** - Live2D character rendering
2. **HeartEmitter** - Animated heart particles
3. **Agora Service** - Audio streaming
4. **API Services** - Agent lifecycle management

### Key State Management
```typescript
- isConnected: boolean          // Call active
- isMuted: boolean              // Microphone state
- isConnecting: boolean         // Transition state
- isAssistantSpeaking: boolean  // Visual feedback
- callDuration: number          // Timer (seconds)
- remoteAudioTrack: any        // Agora audio
```

### API Endpoints
```typescript
/api/token/generate    // Get Agora credentials
/start                 // Start companion agent
/stop                  // Stop companion agent
/ping                  // Keep session alive
```

## 🎯 User Experience Flow

### 1. Initial Load
```
User opens app
   ↓
Shows companion in circle
   ↓
Displays "Tap to call" message
   ↓
Waits for user interaction
```

### 2. Making a Call
```
User taps green call button
   ↓
Shows "Connecting..." status
   ↓
Fetches Agora credentials
   ↓
Connects to audio channel
   ↓
Starts companion agent
   ↓
Changes to "Call in Progress"
   ↓
Starts call timer
   ↓
Enables mic and end call buttons
```

### 3. During Call
```
User speaks
   ↓
Audio streams to companion
   ↓
Companion processes and responds
   ↓
Response audio received
   ↓
Live2D character lip-syncs
   ↓
Green ring glows during speech
   ↓
Hearts emit from character
```

### 4. Ending Call
```
User taps red end call button
   ↓
Shows "Disconnecting..." briefly
   ↓
Stops agent service
   ↓
Disconnects from Agora
   ↓
Returns to idle state
   ↓
Resets call timer
```

## 🎨 Customization Options

### Change Character Model
Edit in `page.tsx`:
```typescript
const defaultModel: Live2DModel = {
  id: "custom_model",
  name: "Your Character Name",
  path: "/models/your_model/model3.json",
  preview: "/models/your_model/preview.png",
};
```

### Adjust Colors
Modify gradient in `page.tsx`:
```typescript
// Background gradient
className="bg-gradient-to-br from-purple-900 via-indigo-900 to-blue-900"

// Change to warm tones:
className="bg-gradient-to-br from-orange-900 via-red-900 to-pink-900"
```

### Modify Call Button Style
```typescript
// Current: Green call button
className="bg-green-500 hover:bg-green-600"

// Alternative: Blue
className="bg-blue-500 hover:bg-blue-600"
```

## 📊 Performance Considerations

### Optimizations
1. **Dynamic imports** - Live2D loads only on client
2. **Animation throttling** - Speaking detection at 160ms intervals
3. **Conditional rendering** - UI elements load based on state
4. **Cleanup on unmount** - Prevents memory leaks

### Best Practices
- Use `backdrop-blur` sparingly
- Limit animated elements on screen
- Debounce state updates
- Clean up intervals and timers
- Reset audio tracks properly

## 🔮 Future Enhancements

- [ ] Video call mode with webcam
- [ ] Multiple character selection
- [ ] Custom ringtones
- [ ] Call history log
- [ ] Voice commands during call
- [ ] Background blur options
- [ ] Dark/light mode toggle
- [ ] Accessibility improvements
- [ ] Mobile-responsive layout
- [ ] Screen recording during calls

## 📱 Mobile Responsiveness

The interface is fully responsive:
- Scales to fit any screen size
- Touch-optimized buttons (min 44x44px)
- No hover effects on mobile
- Proper viewport meta tags
- Prevents zoom on input focus

## 🎬 Demo Flow

1. **Open app** → See circular Live2D character
2. **Tap green phone button** → Initiates connection
3. **Wait 2-3 seconds** → Connection establishes
4. **Start speaking** → See character respond with lip-sync
5. **Watch timer** → Tracks call duration
6. **Toggle mic** → Mute/unmute during call
7. **Tap red button** → Ends call smoothly

## 🙏 Credits

- **Live2D Framework** - Character animations
- **PIXI.js** - Rendering engine
- **Agora** - Real-time voice communication
- **TEN Framework** - Agent orchestration
- **Next.js** - React framework
- **Tailwind CSS** - Styling system
