"use client";

import { useCallback, useEffect, useRef, useState } from "react";
import {
  apiGenAgoraData,
  apiStartService,
  apiStopService,
} from "../common/request";

type ChatItem = {
  id: string;
  text: string;
  isFinal: boolean;
  role: "user" | "assistant";
  ts: number;
};

type TextChunk = {
  message_id: string;
  part_index: number;
  total_parts: number;
  content: string;
};

const generateUserId = () => {
  if (typeof window !== "undefined" && window.crypto?.getRandomValues) {
    const array = new Uint32Array(1);
    window.crypto.getRandomValues(array);
    return 100000 + (array[0] % 900000);
  }
  const fallback = Date.now() % 900000;
  return 100000 + fallback;
};

export default function HomePage() {
  const [mounted, setMounted] = useState(false);
  const [channel, setChannel] = useState<string>("ten_transcription");
  const [userId, setUserId] = useState<number>(0);
  const [joined, setJoined] = useState<boolean>(false);
  const [items, setItems] = useState<ChatItem[]>([]);
  const [error, setError] = useState<string | null>(null);
  const clientRef = useRef<any | null>(null);
  const audioRef = useRef<any | null>(null);
  const cacheRef = useRef<Record<string, TextChunk[]>>({});

  const appendItem = useCallback((it: ChatItem) => {
    setItems((prev) => {
      // merge partials by message_id when possible
      return [...prev, it];
    });
  }, []);

  const handleStreamMessage = useCallback(
    (data: any) => {
      try {
        const ascii = String.fromCharCode(...new Uint8Array(data));
        const [message_id, partIndexStr, totalPartsStr, content] =
          ascii.split("|");
        const part_index = parseInt(partIndexStr, 10);
        const total_parts =
          totalPartsStr === "???" ? -1 : parseInt(totalPartsStr, 10);
        if (total_parts === -1) return;

        const chunk: TextChunk = {
          message_id,
          part_index,
          total_parts,
          content,
        };
        const cache = cacheRef.current;
        if (!cache[message_id]) cache[message_id] = [];
        cache[message_id].push(chunk);

        if (cache[message_id].length === total_parts) {
          const msg = reconstructMessage(cache[message_id]);
          const payload = JSON.parse(base64ToUtf8(msg));
          const { text, is_final, text_ts, role } = payload;
          if (text && String(text).trim().length > 0) {
            appendItem({
              id: message_id,
              text,
              isFinal: !!is_final,
              role: role || "assistant",
              ts: text_ts,
            });
          }
          delete cache[message_id];
        }
      } catch (e) {
        console.warn("failed to parse stream-message", e);
      }
    },
    [appendItem]
  );

  const join = useCallback(async () => {
    if (joined) return;
    const { ok, code, data, msg } = await apiGenAgoraData({ channel, userId });
    if (!ok) {
      console.error("[UI] Failed to get Agora token", { code, msg, data });
      setError(`Token error: ${String(msg)} (code=${String(code)})`);
      return;
    }

    const { default: AgoraRTC } = await import("agora-rtc-sdk-ng");
    const client = AgoraRTC.createClient({ mode: "rtc", codec: "vp8" });
    clientRef.current = client;
    client.on("stream-message", (_uid: any, stream: any) =>
      handleStreamMessage(stream)
    );
    // volume indicator for local + remote tracks
    try {
      client.enableAudioVolumeIndicator?.();
      client.on("volume-indicator", (vols: any[]) => {
        const me = vols.find((v) => String(v.uid) === String(userId));
        if (me) console.log("[UI] Local volume level", me.level);
      });
    } catch {}
    console.log("[UI] Joining channel", { appId: data.appId, channel, userId });
    await client.join(data.appId, channel, data.token, userId);

    const audio = await AgoraRTC.createMicrophoneAudioTrack();
    audioRef.current = audio;
    console.log("[UI] Publishing mic track...");
    await client.publish([audio]);
    console.log("[UI] Mic published");
    setJoined(true);
  }, [channel, userId, joined, handleStreamMessage]);

  const start = useCallback(async () => {
    await apiStartService({ channel, userId, graphName: "transcription" });
    await join();
  }, [channel, userId, join]);

  const stop = useCallback(async () => {
    try {
      // Stop server-side worker
      try {
        await apiStopService(channel);
      } catch {}
      audioRef.current?.close();
      audioRef.current = null;
      if (clientRef.current) {
        await clientRef.current.leave();
        clientRef.current.removeAllListeners();
        clientRef.current = null;
      }
    } finally {
      setJoined(false);
    }
  }, [channel]);

  useEffect(
    () => () => {
      // cleanup
      if (audioRef.current) audioRef.current.close();
      if (clientRef.current) clientRef.current.leave();
    },
    []
  );

  // mount gate to avoid SSR hydration mismatch and set stable random userId
  useEffect(() => {
    setMounted(true);
    if (!userId) {
      const saved = Number(localStorage.getItem("uid") || "0");
      const id = saved || generateUserId();
      setUserId(id);
      localStorage.setItem("uid", String(id));
    }
  }, [userId]);

  if (!mounted) return null;

  return (
    <div
      style={{
        maxWidth: 840,
        margin: "40px auto",
        padding: 16,
        fontFamily: "Inter, system-ui, Arial",
      }}
    >
      <h1 style={{ fontSize: 24, fontWeight: 600 }}>Transcription</h1>
      <p style={{ color: "#666" }}>
        Join the channel and stream your mic; transcripts appear below.
      </p>
      {error && (
        <div style={{ marginTop: 8, color: "#b00020" }}>Error: {error}</div>
      )}
      <div
        style={{ display: "flex", gap: 8, alignItems: "center", marginTop: 12 }}
      >
        <label>Channel</label>
        <input
          value={channel}
          onChange={(e) => setChannel(e.target.value)}
          style={{
            padding: 8,
            flex: 1,
            border: "1px solid #ddd",
            borderRadius: 6,
          }}
        />
        <label>User</label>
        <input
          value={userId}
          onChange={(e) => setUserId(parseInt(e.target.value || "0", 10) || 0)}
          style={{
            width: 120,
            padding: 8,
            border: "1px solid #ddd",
            borderRadius: 6,
          }}
        />
        {!joined ? (
          <button
            onClick={start}
            disabled={!userId || !channel}
            style={{
              padding: "8px 14px",
              background: !userId || !channel ? "#888" : "#111",
              color: "#fff",
              borderRadius: 6,
            }}
          >
            Start
          </button>
        ) : (
          <button
            onClick={stop}
            style={{
              padding: "8px 14px",
              background: "#e33",
              color: "#fff",
              borderRadius: 6,
            }}
          >
            Stop
          </button>
        )}
      </div>

      <div
        style={{
          marginTop: 20,
          border: "1px solid #eee",
          borderRadius: 8,
          padding: 12,
          minHeight: 240,
        }}
      >
        {items.length === 0 && (
          <div style={{ color: "#999" }}>No transcript yet…</div>
        )}
        {items.map((it) => (
          <div
            key={it.id}
            style={{
              padding: "6px 0",
              color: it.role === "assistant" ? "#222" : "#555",
            }}
          >
            <span style={{ fontSize: 12, color: "#999", marginRight: 8 }}>
              {new Date(it.ts).toLocaleTimeString()}
            </span>
            <strong style={{ marginRight: 6 }}>
              {it.role === "assistant" ? "AI" : "You"}:
            </strong>
            <span>{it.text}</span>
            {!it.isFinal && (
              <em style={{ color: "#999", marginLeft: 8 }}>(…)</em>
            )}
          </div>
        ))}
      </div>
    </div>
  );
}

function reconstructMessage(chunks: TextChunk[]): string {
  chunks.sort((a, b) => a.part_index - b.part_index);
  return chunks.map((c) => c.content).join("");
}

function base64ToUtf8(base64: string): string {
  const binaryString = atob(base64);
  const bytes = new Uint8Array(binaryString.length);
  for (let i = 0; i < binaryString.length; i++)
    bytes[i] = binaryString.charCodeAt(i);
  return new TextDecoder("utf-8").decode(bytes);
}
