import axios from "axios";
import type { AgentStatus } from "@/types";

export class AgentService {
  private baseUrl: string;
  private status: AgentStatus = {
    isRunning: false,
    isConnected: false,
  };

  // Event callbacks
  private onStatusChange?: (status: AgentStatus) => void;

  constructor(baseUrl: string = "http://localhost:8080") {
    this.baseUrl = baseUrl;
  }

  async startAgent(
    channelName: string = "live2d-voice-assistant",
    userUid: number = 12345
  ): Promise<boolean> {
    try {
      const response = await axios.post(`${this.baseUrl}/start`, {
        request_id: `start-${Date.now()}`,
        channel_name: channelName,
        user_uid: userUid,
        graph_name: "camera_va_openai_azure",
        properties: {
          openai_chatgpt: {
            model: "gpt-4o",
          },
        },
      });

      if (response.status === 200) {
        this.status = {
          isRunning: true,
          isConnected: true,
          lastPing: new Date(),
        };
        this.onStatusChange?.(this.status);
        return true;
      }

      return false;
    } catch (error) {
      console.error("Failed to start agent:", error);
      this.status = {
        isRunning: false,
        isConnected: false,
        error: error instanceof Error ? error.message : "Unknown error",
      };
      this.onStatusChange?.(this.status);
      return false;
    }
  }

  async stopAgent(
    channelName: string = "live2d-voice-assistant"
  ): Promise<boolean> {
    try {
      const response = await axios.post(`${this.baseUrl}/stop`, {
        request_id: `stop-${Date.now()}`,
        channel_name: channelName,
      });

      if (response.status === 200) {
        this.status = {
          isRunning: false,
          isConnected: false,
        };
        this.onStatusChange?.(this.status);
        return true;
      }

      return false;
    } catch (error) {
      console.error("Failed to stop agent:", error);
      this.status = {
        isRunning: false,
        isConnected: false,
        error: error instanceof Error ? error.message : "Unknown error",
      };
      this.onStatusChange?.(this.status);
      return false;
    }
  }

  async pingAgent(
    channelName: string = "live2d-voice-assistant"
  ): Promise<boolean> {
    try {
      const response = await axios.post(`${this.baseUrl}/ping`, {
        request_id: `ping-${Date.now()}`,
        channel_name: channelName,
      });

      if (response.status === 200) {
        this.status = {
          isRunning: true,
          isConnected: true,
          lastPing: new Date(),
        };
        this.onStatusChange?.(this.status);
        return true;
      }

      return false;
    } catch (error) {
      console.error("Failed to ping agent:", error);
      this.status = {
        isRunning: false,
        isConnected: false,
        error: error instanceof Error ? error.message : "Unknown error",
      };
      this.onStatusChange?.(this.status);
      return false;
    }
  }

  async getAgentStatus(): Promise<AgentStatus> {
    try {
      const response = await axios.get(`${this.baseUrl}/list`);

      if (response.status === 200) {
        // Check if our channel is in the list of running agents
        const agents = response.data.data || [];
        const isRunning = agents.some(
          (agent: any) => agent.channelName === "live2d-voice-assistant"
        );

        this.status = {
          isRunning,
          isConnected: isRunning,
          lastPing: isRunning ? new Date() : undefined,
          error: undefined,
        };
        this.onStatusChange?.(this.status);
        return this.status;
      }

      return this.status;
    } catch (error) {
      console.error("Failed to get agent status:", error);
      this.status = {
        isRunning: false,
        isConnected: false,
        error: error instanceof Error ? error.message : "Unknown error",
      };
      this.onStatusChange?.(this.status);
      return this.status;
    }
  }

  // Getters
  getStatus(): AgentStatus {
    return this.status;
  }

  // Event setters
  setOnStatusChange(callback: (status: AgentStatus) => void) {
    this.onStatusChange = callback;
  }
}

// Singleton instance
export const agentService = new AgentService();
