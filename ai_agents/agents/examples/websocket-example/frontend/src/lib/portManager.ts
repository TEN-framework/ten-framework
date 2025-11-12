/**
 * Port Manager - Handles WebSocket port assignment with localStorage persistence
 */

const PORT_STORAGE_KEY = "websocket_port";
const PORT_MIN = 8000;
const PORT_MAX = 9000;

/**
 * Generate a random port number in the range 8000-9000
 */
export function generateRandomPort(): number {
  return Math.floor(Math.random() * (PORT_MAX - PORT_MIN + 1)) + PORT_MIN;
}

/**
 * Get the stored port from localStorage, or generate a new one if not exists
 */
export function getOrGeneratePort(): number {
  if (typeof window === "undefined") {
    // SSR context - return default port
    return 8765;
  }

  const stored = localStorage.getItem(PORT_STORAGE_KEY);
  if (stored) {
    const port = parseInt(stored, 10);
    if (!isNaN(port) && port >= PORT_MIN && port <= PORT_MAX) {
      return port;
    }
  }

  // Generate and save new port
  const newPort = generateRandomPort();
  savePort(newPort);
  return newPort;
}

/**
 * Save the port to localStorage
 */
export function savePort(port: number): void {
  if (typeof window === "undefined") {
    return;
  }

  localStorage.setItem(PORT_STORAGE_KEY, port.toString());
}

/**
 * Clear the stored port (useful for troubleshooting)
 */
export function clearPort(): void {
  if (typeof window === "undefined") {
    return;
  }

  localStorage.removeItem(PORT_STORAGE_KEY);
}

/**
 * Get the WebSocket URL for a given port
 */
export function getWebSocketUrl(port: number): string {
  return `ws://localhost:${port}`;
}
