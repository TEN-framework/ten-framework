/**
 * API utility functions for agent lifecycle management
 */

import axios from "axios";

/**
 * Generate a UUID (v4)
 */
export function genUUID(): string {
  return "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx".replace(/[xy]/g, (c) => {
    const r = (Math.random() * 16) | 0;
    const v = c === "x" ? r : (r & 0x3) | 0x8;
    return v.toString(16);
  });
}

/**
 * Send a ping to keep the agent alive
 */
export async function ping(channel: string): Promise<void> {
  const url = `/api/agents/ping`;
  const data = {
    request_id: genUUID(),
    channel_name: channel,
  };

  try {
    await axios.post(url, data);
  } catch (error) {
    console.error("Ping failed:", error);
    // Don't throw - pings should be non-blocking
  }
}
