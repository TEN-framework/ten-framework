"use client";

import { useState } from "react";
import DailyIframe from "@daily-co/daily-js";

export default function TavusPage() {
  const [callFrame, setCallFrame] = useState<any>(null);
  const [conversationUrl, setConversationUrl] = useState("");
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState("");

  const createConversation = async () => {
    setIsLoading(true);
    setError("");

    try {
      // Call TEN API to create Tavus conversation
      const response = await fetch("http://localhost:8080/api/tavus/conversation/create", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({}),
      });

      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`);
      }

      const data = await response.json();

      if (data.code !== "0") {
        throw new Error(data.msg || "Failed to create conversation");
      }

      const { conversation_url } = data.data;
      setConversationUrl(conversation_url);

      // Create Daily.co iframe and join the conversation
      const daily = DailyIframe.createFrame({
        showLeaveButton: true,
        iframeStyle: {
          position: "relative",
          width: "100%",
          height: "600px",
          border: "0",
          borderRadius: "8px",
        },
      });

      await daily.join({ url: conversation_url });
      setCallFrame(daily);

      // Listen for left-meeting event
      daily.on("left-meeting", () => {
        daily.destroy();
        setCallFrame(null);
        setConversationUrl("");
      });
    } catch (err: any) {
      console.error("Failed to create conversation:", err);
      setError(err.message || "Failed to create conversation. Please check your API key and try again.");
    } finally {
      setIsLoading(false);
    }
  };

  const endConversation = () => {
    if (callFrame) {
      callFrame.leave();
      callFrame.destroy();
      setCallFrame(null);
      setConversationUrl("");
    }
  };

  return (
    <div style={{
      minHeight: "100vh",
      backgroundColor: "#0f0f0f",
      color: "#ffffff",
      padding: "2rem",
      fontFamily: "system-ui, -apple-system, sans-serif",
    }}>
      <div style={{ maxWidth: "1200px", margin: "0 auto" }}>
        {/* Header */}
        <div style={{ marginBottom: "3rem" }}>
          <h1 style={{
            fontSize: "2.5rem",
            fontWeight: "bold",
            marginBottom: "0.5rem",
          }}>
            Tavus Digital Human Demo
          </h1>
          <p style={{
            fontSize: "1.125rem",
            color: "#888",
          }}>
            Interact with an AI-powered digital human using natural conversation
          </p>
        </div>

        {/* Error Message */}
        {error && (
          <div style={{
            backgroundColor: "#ff4444",
            padding: "1rem",
            borderRadius: "8px",
            marginBottom: "2rem",
          }}>
            <strong>Error:</strong> {error}
          </div>
        )}

        {/* Main Content */}
        <div style={{
          backgroundColor: "#1a1a1a",
          borderRadius: "12px",
          padding: "2rem",
          boxShadow: "0 4px 6px rgba(0, 0, 0, 0.3)",
        }}>
          {!callFrame ? (
            <div style={{ textAlign: "center", padding: "3rem 0" }}>
              <p style={{
                fontSize: "1.125rem",
                marginBottom: "2rem",
                color: "#ccc",
              }}>
                Click the button below to start a conversation with your AI digital human
              </p>
              <button
                onClick={createConversation}
                disabled={isLoading}
                style={{
                  backgroundColor: isLoading ? "#555" : "#0066ff",
                  color: "#ffffff",
                  padding: "1rem 2rem",
                  fontSize: "1.125rem",
                  fontWeight: "600",
                  border: "none",
                  borderRadius: "8px",
                  cursor: isLoading ? "not-allowed" : "pointer",
                  transition: "background-color 0.2s",
                }}
                onMouseEnter={(e) => {
                  if (!isLoading) {
                    e.currentTarget.style.backgroundColor = "#0052cc";
                  }
                }}
                onMouseLeave={(e) => {
                  if (!isLoading) {
                    e.currentTarget.style.backgroundColor = "#0066ff";
                  }
                }}
              >
                {isLoading ? "Creating Conversation..." : "Start Conversation"}
              </button>
            </div>
          ) : (
            <div>
              <div style={{ marginBottom: "1rem", textAlign: "right" }}>
                <button
                  onClick={endConversation}
                  style={{
                    backgroundColor: "#ff4444",
                    color: "#ffffff",
                    padding: "0.75rem 1.5rem",
                    fontSize: "1rem",
                    fontWeight: "600",
                    border: "none",
                    borderRadius: "8px",
                    cursor: "pointer",
                    transition: "background-color 0.2s",
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.backgroundColor = "#cc0000";
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = "#ff4444";
                  }}
                >
                  End Conversation
                </button>
              </div>
              <div id="daily-container" />
            </div>
          )}
        </div>

        {/* Instructions */}
        <div style={{
          marginTop: "3rem",
          padding: "2rem",
          backgroundColor: "#1a1a1a",
          borderRadius: "12px",
        }}>
          <h2 style={{
            fontSize: "1.5rem",
            fontWeight: "600",
            marginBottom: "1rem",
          }}>
            How to Use
          </h2>
          <ol style={{
            lineHeight: "1.8",
            color: "#ccc",
            paddingLeft: "1.5rem",
          }}>
            <li>Click "Start Conversation" to create a new session with the AI digital human</li>
            <li>Allow microphone and camera access when prompted</li>
            <li>Speak naturally - the AI will listen and respond in real-time</li>
            <li>Click "End Conversation" when you're done</li>
          </ol>

          <div style={{ marginTop: "2rem", paddingTop: "2rem", borderTop: "1px solid #333" }}>
            <h3 style={{ fontSize: "1.125rem", fontWeight: "600", marginBottom: "0.5rem" }}>
              Requirements
            </h3>
            <ul style={{ lineHeight: "1.8", color: "#ccc", paddingLeft: "1.5rem" }}>
              <li>TAVUS_API_KEY must be set in your .env file</li>
              <li>Supported browsers: Chrome, Firefox, Safari (latest versions)</li>
              <li>Stable internet connection required for best experience</li>
            </ul>
          </div>
        </div>
      </div>
    </div>
  );
}
