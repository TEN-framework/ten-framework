"use client";

import { useState, useEffect } from "react";

export default function TavusPage() {
  const [callFrame, setCallFrame] = useState<any>(null);
  const [conversationUrl, setConversationUrl] = useState("");
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState("");
  const [mounted, setMounted] = useState(false);

  useEffect(() => {
    setMounted(true);
  }, []);

  if (!mounted) {
    return null;
  }

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
      const { default: DailyIframe } = await import("@daily-co/daily-js");
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
      backgroundColor: "#fafafa",
      display: "flex",
      alignItems: "center",
      justifyContent: "center",
      padding: "2rem",
      fontFamily: "-apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif",
    }}>
      <div style={{
        maxWidth: "900px",
        width: "100%",
      }}>
        {/* Header */}
        <div style={{
          textAlign: "center",
          marginBottom: "3rem"
        }}>
          <h1 style={{
            fontSize: "2.25rem",
            fontWeight: "300",
            color: "#1a1a1a",
            marginBottom: "0.75rem",
            letterSpacing: "-0.02em",
          }}>
            Tavus
          </h1>
          <p style={{
            fontSize: "0.95rem",
            color: "#666",
            fontWeight: "400",
          }}>
            Conversational digital human
          </p>
        </div>

        {/* Error Message */}
        {error && (
          <div style={{
            backgroundColor: "#fff5f5",
            border: "1px solid #fed7d7",
            color: "#c53030",
            padding: "0.875rem 1.25rem",
            borderRadius: "6px",
            marginBottom: "2rem",
            fontSize: "0.9rem",
          }}>
            {error}
          </div>
        )}

        {/* Main Content */}
        <div style={{
          backgroundColor: "#ffffff",
          borderRadius: "12px",
          padding: callFrame ? "1.5rem" : "4rem 2rem",
          boxShadow: "0 1px 3px rgba(0, 0, 0, 0.08), 0 1px 2px rgba(0, 0, 0, 0.04)",
          border: "1px solid #e5e5e5",
        }}>
          {!callFrame ? (
            <div style={{ textAlign: "center" }}>
              <button
                onClick={createConversation}
                disabled={isLoading}
                style={{
                  backgroundColor: isLoading ? "#e0e0e0" : "#000",
                  color: isLoading ? "#999" : "#fff",
                  padding: "0.875rem 2.5rem",
                  fontSize: "0.95rem",
                  fontWeight: "500",
                  border: "none",
                  borderRadius: "6px",
                  cursor: isLoading ? "not-allowed" : "pointer",
                  transition: "all 0.15s ease",
                  letterSpacing: "0.01em",
                }}
              >
                {isLoading ? "Starting..." : "Start Conversation"}
              </button>
            </div>
          ) : (
            <div>
              <div style={{
                marginBottom: "1rem",
                display: "flex",
                justifyContent: "flex-end",
              }}>
                <button
                  onClick={endConversation}
                  style={{
                    backgroundColor: "#fff",
                    color: "#666",
                    padding: "0.5rem 1.25rem",
                    fontSize: "0.875rem",
                    fontWeight: "500",
                    border: "1px solid #d0d0d0",
                    borderRadius: "6px",
                    cursor: "pointer",
                    transition: "all 0.15s ease",
                  }}
                >
                  End
                </button>
              </div>
              <div id="daily-container" style={{
                borderRadius: "8px",
                overflow: "hidden",
              }} />
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
