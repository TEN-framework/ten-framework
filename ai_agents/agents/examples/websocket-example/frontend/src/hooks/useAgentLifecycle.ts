/**
 * Agent Lifecycle Hook - Manages agent start/stop via API server
 */

import { useCallback, useEffect, useRef, useState } from "react";
import { ping } from "@/lib/api";

interface StartAgentParams {
  port: number;
  graphName?: string;
  timeout?: number;
}

interface AgentState {
  status: "idle" | "starting" | "running" | "error" | "stopped";
  error: string | null;
  channelName: string | null;
  requestId: string | null;
}

interface StartAgentResponse {
  code: number | string;
  msg: string;
  data?: any;
}

/**
 * Hook to manage agent lifecycle (start/stop)
 */
export function useAgentLifecycle() {
  const [state, setState] = useState<AgentState>({
    status: "idle",
    error: null,
    channelName: null,
    requestId: null,
  });

  // Ping interval ref to keep agent alive
  const pingIntervalRef = useRef<NodeJS.Timeout | null>(null);

  /**
   * Start pinging the agent to keep it alive
   * Uses recursive setTimeout to prevent ping stacking
   */
  const startPing = useCallback((channelName: string) => {
    // Clear any existing timeout
    if (pingIntervalRef.current) {
      clearTimeout(pingIntervalRef.current);
    }

    // Recursive setTimeout pattern - waits for completion before next ping
    const schedulePing = async () => {
      await ping(channelName);
      // Schedule next ping only after current one completes
      pingIntervalRef.current = setTimeout(schedulePing, 3000);
    };

    // Start the first ping
    schedulePing();
  }, []);

  /**
   * Stop pinging the agent
   */
  const stopPing = useCallback(() => {
    if (pingIntervalRef.current) {
      clearTimeout(pingIntervalRef.current);
      pingIntervalRef.current = null;
    }
  }, []);

  // Clean up ping interval on unmount
  useEffect(() => {
    return () => {
      stopPing();
    };
  }, [stopPing]);

  /**
   * Generate a unique channel name
   */
  const generateChannelName = useCallback(() => {
    return `websocket-${Date.now()}-${Math.random().toString(36).substring(7)}`;
  }, []);

  /**
   * Generate a unique request ID
   */
  const generateRequestId = useCallback(() => {
    return `${Date.now()}-${Math.random().toString(36).substring(2)}`;
  }, []);

  /**
   * Start the agent with property overrides
   */
  const startAgent = useCallback(
    async ({
      port,
      graphName = "voice_assistant",
      timeout = -1,
    }: StartAgentParams) => {
      setState((prev) => ({ ...prev, status: "starting", error: null }));

      const channelName = generateChannelName();
      const requestId = generateRequestId();

      try {
        const response = await fetch("/api/agents/start", {
          method: "POST",
          headers: {
            "Content-Type": "application/json",
          },
          body: JSON.stringify({
            request_id: requestId,
            channel_name: channelName,
            user_uid: 176573,
            graph_name: graphName,
            properties: {
              websocket_server: {
                port: port,
              },
            },
            timeout: timeout,
          }),
        });

        const data: StartAgentResponse = await response.json();

        // Handle both string and number codes (server returns "0" as string for success)
        const code =
          typeof data.code === "string" ? parseInt(data.code, 10) : data.code;
        if (!response.ok || code !== 0) {
          throw new Error(data.msg || "Failed to start agent");
        }

        setState({
          status: "running",
          error: null,
          channelName,
          requestId,
        });

        // Start pinging to keep agent alive
        startPing(channelName);

        return { channelName, requestId, port };
      } catch (error) {
        const errorMessage =
          error instanceof Error ? error.message : "Unknown error";
        setState({
          status: "error",
          error: errorMessage,
          channelName: null,
          requestId: null,
        });
        throw error;
      }
    },
    [generateChannelName, generateRequestId, startPing]
  );

  /**
   * Stop the agent
   */
  const stopAgent = useCallback(async () => {
    if (!state.channelName) {
      console.warn("No active agent to stop");
      return;
    }

    try {
      // Stop pinging
      stopPing();

      const response = await fetch("/api/agents/stop", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({
          channel_name: state.channelName,
        }),
      });

      if (!response.ok) {
        throw new Error("Failed to stop agent");
      }

      setState({
        status: "stopped",
        error: null,
        channelName: null,
        requestId: null,
      });
    } catch (error) {
      console.error("Error stopping agent:", error);
      // Don't update state to error - stopping is best effort
    }
  }, [state.channelName, stopPing]);

  /**
   * Reset the state
   */
  const reset = useCallback(() => {
    stopPing();
    setState({
      status: "idle",
      error: null,
      channelName: null,
      requestId: null,
    });
  }, [stopPing]);

  return {
    state,
    startAgent,
    stopAgent,
    reset,
    isStarting: state.status === "starting",
    isRunning: state.status === "running",
    hasError: state.status === "error",
  };
}
