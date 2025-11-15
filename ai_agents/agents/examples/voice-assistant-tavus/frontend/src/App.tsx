import { clsx } from "clsx";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";

type SessionState = "idle" | "starting" | "running" | "stopping" | "error";

interface EventEntry {
  id: string;
  timestamp: number;
  label: string;
  body?: string;
}

const GRAPH_NAME = "voice_assistant_tavus";
const API_BASE =
  import.meta.env.VITE_API_BASE_URL?.replace(/\/$/, "") ||
  "http://localhost:8080";
const WS_URL =
  import.meta.env.VITE_WS_URL || "ws://localhost:8765";

export default function App() {
  const [channelName, setChannelName] = useState("tavus_showcase");
  const [sessionState, setSessionState] = useState<SessionState>("idle");
  const [conversationUrl, setConversationUrl] = useState<string>();
  const [personaId, setPersonaId] = useState<string>();
  const [events, setEvents] = useState<EventEntry[]>([]);
  const [wsConnected, setWsConnected] = useState(false);
  const reconnectRef = useRef<number>();

  const addEvent = useCallback((label: string, body?: string) => {
    setEvents((prev) => {
      const entry: EventEntry = {
        id: crypto.randomUUID(),
        timestamp: Date.now(),
        label,
        body
      };
      return [entry, ...prev].slice(0, 50);
    });
  }, []);

  const handleMessage = useCallback(
    (payload: any) => {
      if (payload?.type === "data" && payload?.name === "text_data") {
        const data = payload.data;
        if (data?.data_type === "tavus_event") {
          const details = JSON.stringify(data.payload, null, 2);
          if (data.event === "persona_created") {
            setPersonaId(data.payload?.persona_id);
            addEvent("Persona created", details);
          } else if (data.event === "conversation_created") {
            setConversationUrl(data.payload?.conversation_url);
            addEvent("Conversation ready", details);
            setSessionState("running");
          } else if (data.event === "conversation_ended") {
            setConversationUrl(undefined);
            addEvent("Conversation ended", details);
            setSessionState("idle");
          } else {
            addEvent(`Event: ${data.event}`, details);
          }
          return;
        }
      }

      if (payload?.name === "tavus_conversation_created") {
        const url = payload?.data?.conversation_url;
        if (url) {
          setConversationUrl(url);
          addEvent("Conversation ready", url);
        }
      }
    },
    [addEvent]
  );

  useEffect(() => {
    let ws: WebSocket | null = null;

    const connect = () => {
      ws = new WebSocket(WS_URL);
      ws.onopen = () => {
        setWsConnected(true);
        addEvent("Connected to TEN websocket");
      };
      ws.onclose = () => {
        setWsConnected(false);
        addEvent("Websocket disconnected, retrying...");
        reconnectRef.current = window.setTimeout(connect, 2000);
      };
      ws.onerror = () => {
        setWsConnected(false);
      };
      ws.onmessage = (event) => {
        try {
          const payload = JSON.parse(event.data);
          handleMessage(payload);
        } catch (err) {
          console.error("Failed to parse websocket payload", err);
        }
      };
    };

    connect();
    return () => {
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.close();
      }
      if (reconnectRef.current) {
        clearTimeout(reconnectRef.current);
      }
    };
  }, [addEvent, handleMessage]);

  const apiRequest = useCallback(
    async (
      path: string,
      body: Record<string, unknown>
    ): Promise<Response> => {
      const response = await fetch(`${API_BASE}${path}`, {
        method: "POST",
        headers: {
          "Content-Type": "application/json"
        },
        body: JSON.stringify(body)
      });
      if (!response.ok) {
        const text = await response.text();
        throw new Error(text || response.statusText);
      }
      return response;
    },
    []
  );

  const startSession = useCallback(async () => {
    if (sessionState !== "idle") return;
    setSessionState("starting");
    addEvent("Booting TEN agent...");
    try {
      const requestId = crypto.randomUUID();
      await apiRequest("/start", {
        request_id: requestId,
        channel_name: channelName,
        graph_name: GRAPH_NAME,
        timeout: 3600
      });
      addEvent("TEN agent launched", `Channel: ${channelName}`);
    } catch (err) {
      console.error(err);
      addEvent("Failed to launch agent", String(err));
      setSessionState("error");
      setTimeout(() => setSessionState("idle"), 2000);
    }
  }, [addEvent, apiRequest, channelName, sessionState]);

  const stopSession = useCallback(async () => {
    if (sessionState === "idle" || sessionState === "stopping") return;
    setSessionState("stopping");
    addEvent("Stopping TEN agent...");
    try {
      await apiRequest("/stop", {
        request_id: crypto.randomUUID(),
        channel_name: channelName
      });
      setConversationUrl(undefined);
      addEvent("TEN agent stopped");
      setSessionState("idle");
    } catch (err) {
      console.error(err);
      addEvent("Failed to stop agent", String(err));
      setSessionState("error");
    }
  }, [addEvent, apiRequest, channelName, sessionState]);

  const statusLabel = useMemo(() => {
    switch (sessionState) {
      case "starting":
        return "Starting";
      case "running":
        return "Running";
      case "stopping":
        return "Stopping";
      case "error":
        return "Error";
      default:
        return "Idle";
    }
  }, [sessionState]);

  return (
    <div className="app-shell">
      <header className="hero">
        <div>
          <p className="eyebrow">TEN x Tavus</p>
          <h1>Bring lifelike Tavus personas into any experience.</h1>
          <p className="muted">
            Launch the TEN agent, wait for the websocket event, and meet your
            Tavus persona inside this immersive canvas. No playground UI
            required.
          </p>
        </div>
        <div className="panel">
          <div className="field">
            <label htmlFor="channel">Channel name</label>
            <input
              id="channel"
              value={channelName}
              onChange={(e) => setChannelName(e.target.value)}
              placeholder="tavus_showcase"
            />
          </div>
          <div className="status-row">
            <span className={clsx("status-pill", sessionState)}>
              {statusLabel}
            </span>
            <span
              className={clsx("status-pill", wsConnected ? "running" : "error")}
            >
              WS {wsConnected ? "Connected" : "Disconnected"}
            </span>
          </div>
          <div className="actions">
            <button
              className="primary"
              onClick={startSession}
              disabled={sessionState !== "idle"}
            >
              Start Session
            </button>
            <button
              className="ghost"
              onClick={stopSession}
              disabled={sessionState === "idle"}
            >
              Stop
            </button>
          </div>
          <div className="details">
            <p>
              <strong>Graph:</strong> {GRAPH_NAME}
            </p>
            {personaId && (
              <p>
                <strong>Persona:</strong> {personaId}
              </p>
            )}
          </div>
        </div>
      </header>

      <main className="content-grid">
        <section className="card">
          <div className="card-header">
            <div>
              <h2>Conversation Surface</h2>
              <p className="muted">
                The Tavus Conversational Video Interface loads below once the
                agent boots. Grant camera & microphone access when prompted.
              </p>
            </div>
            {conversationUrl && (
              <a
                href={conversationUrl}
                target="_blank"
                rel="noreferrer"
                className="link"
              >
                Open in new tab →
              </a>
            )}
          </div>
          {conversationUrl ? (
            <iframe
              key={conversationUrl}
              src={conversationUrl}
              className="tavus-frame"
              allow="camera; microphone; autoplay; fullscreen"
            />
          ) : (
            <div className="empty-state">
              <p className="muted">
                Start the session to fetch a Tavus room. The iframe will appear
                here automatically.
              </p>
            </div>
          )}
        </section>

        <section className="card">
          <h2>Live Events</h2>
          <div className="event-log">
            {events.length === 0 && (
              <p className="muted">Events will appear here.</p>
            )}
            {events.map((event) => (
              <article key={event.id}>
                <div className="event-meta">
                  <span>{new Date(event.timestamp).toLocaleTimeString()}</span>
                  <strong>{event.label}</strong>
                </div>
                {event.body && (
                  <pre>
                    <code>{event.body}</code>
                  </pre>
                )}
              </article>
            ))}
          </div>
        </section>
      </main>
    </div>
  );
}
