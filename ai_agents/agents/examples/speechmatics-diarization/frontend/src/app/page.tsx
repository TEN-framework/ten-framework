"use client"

import { useCallback, useEffect, useMemo, useRef, useState } from "react"
import type { CSSProperties } from "react"
import type {
  IRemoteAudioTrack,
  IAgoraRTCClient,
  IMicrophoneAudioTrack,
  UID,
} from "agora-rtc-sdk-ng"
import {
  apiGenAgoraData,
  apiPing,
  apiStartService,
  apiStopService,
} from "../common/request"
import Threads from "@/components/Threads"

type ChatItem = {
  id: string
  text: string
  speaker?: string
  isFinal: boolean
  role: "user" | "assistant"
  ts: number
}

type TextChunk = {
  message_id: string
  part_index: number
  total_parts: number
  content: string
}

type StatusTone = "positive" | "neutral" | "warning" | "muted"

const DEFAULT_CHANNEL = "ten_diarization_who_likes_what"

const SPEAKER_REGEX = /^\[([^\]]+)\]\s*/

const KNOWN_SPEAKERS = ["Elliot", "Taytay", "Musk"] as const

const SPEAKER_ACCENTS: Record<string, string> = {
  Elliot: "#111827",
  Taytay: "#4b5563",
  Musk: "#9ca3af",
}

const PAGE_BACKGROUND = "#f8fafc"
const PANEL_BACKGROUND = "#ffffff"
const PANEL_BORDER_COLOR = "#e5e7eb"
const PANEL_SHADOW = "0 10px 30px rgba(15, 23, 42, 0.08)"
const TEXT_PRIMARY = "#111827"
const TEXT_MUTED = "#6b7280"

const panelBaseStyle: CSSProperties = {
  borderRadius: 20,
  border: `1px solid ${PANEL_BORDER_COLOR}`,
  background: PANEL_BACKGROUND,
  boxShadow: PANEL_SHADOW,
  padding: "24px 28px",
  color: TEXT_PRIMARY,
}

const sectionTitleStyle: CSSProperties = {
  margin: 0,
  fontSize: 17,
  fontWeight: 600,
}

const sectionSubtitleStyle: CSSProperties = {
  marginTop: 6,
  fontSize: 14,
  color: TEXT_MUTED,
  maxWidth: 600,
}

const errorBannerStyle: CSSProperties = {
  ...panelBaseStyle,
  padding: "16px 18px",
  background: "#f3f4f6",
  border: "1px solid #d1d5db",
  color: TEXT_PRIMARY,
  display: "flex",
  alignItems: "center",
  gap: 12,
}

const transcriptBorderColor = "#e5e7eb"

const generateUserId = () => {
  if (typeof window !== "undefined" && window.crypto?.getRandomValues) {
    const array = new Uint32Array(1)
    window.crypto.getRandomValues(array)
    return 100000 + (array[0] % 900000)
  }
  const fallback = Date.now() % 900000
  return 100000 + fallback
}

export default function HomePage() {
  const [mounted, setMounted] = useState(false)
  const [channel, setChannel] = useState<string>(DEFAULT_CHANNEL)
  const [userId, setUserId] = useState<number>(0)
  const [joined, setJoined] = useState<boolean>(false)
  const [items, setItems] = useState<ChatItem[]>([])
  const [error, setError] = useState<string | null>(null)
  const [pending, setPending] = useState<boolean>(false)
  const [micEnabled, setMicEnabled] = useState<boolean>(false)
  const [lastReplySpeaker, setLastReplySpeaker] = useState<string | null>(null)
  const [activeSpeaker, setActiveSpeaker] = useState<string | null>(null)

  const clientRef = useRef<IAgoraRTCClient | null>(null)
  const audioRef = useRef<IMicrophoneAudioTrack | null>(null)
  const remoteTracksRef = useRef<Map<string, IRemoteAudioTrack>>(new Map())
  const cacheRef = useRef<Record<string, TextChunk[]>>({})
  const transcriptContainerRef = useRef<HTMLDivElement | null>(null)
  const cardRefs = useRef<Record<string, HTMLDivElement | null>>({})

  const appendOrUpdateItem = useCallback((incoming: ChatItem) => {
    setItems((prev) => {
      const idx = prev.findIndex((item) => item.id === incoming.id)
      if (idx === -1) {
        return [...prev, incoming].sort((a, b) => a.ts - b.ts)
      }
      const next = [...prev]
      next[idx] = { ...next[idx], ...incoming }
      return next.sort((a, b) => a.ts - b.ts)
    })

    if (incoming.speaker) {
      setActiveSpeaker(incoming.speaker)
    }
    if (incoming.role === "assistant" && incoming.isFinal && incoming.speaker) {
      setLastReplySpeaker(incoming.speaker)
    }
  }, [])

  const recognisedSpeakers = useMemo(() => {
    const names = new Set<string>()
    items.forEach((item) => {
      if (item.speaker) {
        names.add(item.speaker)
      }
    })
    return Array.from(names)
  }, [items])
  const lastSpeakerLines = useMemo(() => {
    const entries = new Map<string, string>()
    items.forEach((item) => {
      if (item.speaker && item.text) {
        entries.set(item.speaker, item.text)
      }
    })
    return entries
  }, [items])

  const speakerTranscriptMap = useMemo(() => {
    const map = new Map<string, ChatItem[]>()
    items.forEach((item) => {
      if (!item.speaker || !item.text || !item.isFinal) {
        return
      }
      const current = map.get(item.speaker) || []
      current.push(item)
      map.set(item.speaker, current)
    })
    map.forEach((list, speaker) => {
      const sorted = [...list].sort((a, b) => b.ts - a.ts)
      map.set(speaker, sorted)
    })
    return map
  }, [items])

  const focusSpeaker = activeSpeaker || lastReplySpeaker

  type SpeakerCard = {
    name: string
    accent: string
    transcriptLines: string[]
    tagline: string
    status: "active" | "idle" | "waiting"
  }

  const speakerCards = useMemo<SpeakerCard[]>(() => {
    const currentFocus = focusSpeaker
    const base: SpeakerCard[] = KNOWN_SPEAKERS.map((name) => {
      const recognised = recognisedSpeakers.includes(name)
      const accent = recognised ? SPEAKER_ACCENTS[name] || "#4b5563" : "#d1d5db"
      const transcriptEntries = speakerTranscriptMap.get(name) || []
      const transcriptLines = transcriptEntries
        .slice(0, 4)
        .map((entry) => entry.text?.trim() || "")
        .filter((line) => line.length > 0)
      const fallbackDetail = lastSpeakerLines.get(name)
      if (transcriptLines.length === 0) {
        if (fallbackDetail) {
          transcriptLines.push(fallbackDetail)
        } else if (recognised) {
          transcriptLines.push("Listening for next utterance.")
        } else {
          transcriptLines.push("Waiting for enrollment phrase.")
        }
      }
      const status: "active" | "idle" | "waiting" =
        currentFocus === name
          ? "active"
          : recognised
            ? "idle"
            : "waiting"
      const tagline =
        status === "active"
          ? "Speaking now"
          : status === "idle"
            ? "Ready and enrolled"
            : "Not yet enrolled"
      return {
        name,
        accent,
        transcriptLines,
        tagline,
        status,
      }
    })

    if (base.length <= 1) {
      return base
    }

    if (currentFocus) {
      const activeIndex = base.findIndex((card) => card.name === currentFocus)
      if (activeIndex !== -1) {
        const leftIndex = (activeIndex - 1 + base.length) % base.length
        const rightIndex = (activeIndex + 1) % base.length
        const ordered = [
          base[leftIndex],
          base[activeIndex],
          base[rightIndex],
        ]
        const seen = new Set<string>()
        const unique = ordered.filter((card) => {
          if (seen.has(card.name)) {
            return false
          }
          seen.add(card.name)
          return true
        })
        if (unique.length < base.length) {
          base.forEach((card) => {
            if (!seen.has(card.name)) {
              unique.push(card)
              seen.add(card.name)
            }
          })
        }
        return unique
      }
    }

    return base
  }, [recognisedSpeakers, speakerTranscriptMap, lastSpeakerLines, focusSpeaker, activeSpeaker, lastReplySpeaker])

  useEffect(() => {
    const focus = focusSpeaker
    if (!focus) return
    const node = cardRefs.current[focus]
    if (node) {
      node.scrollIntoView({
        behavior: "smooth",
        inline: "center",
        block: "nearest",
      })
    }
  }, [focusSpeaker, speakerCards])

  useEffect(() => {
    if (typeof window === "undefined") {
      return
    }
    const styleId = "spotlight-slide-style"
    if (document.getElementById(styleId)) {
      return
    }
    const style = document.createElement("style")
    style.id = styleId
    style.innerHTML = `
@keyframes spotlight-slide-in {
  0% {
    transform: translateY(24px);
    opacity: 0;
  }
  60% {
    transform: translateY(-10px);
    opacity: 1;
  }
  100% {
    transform: translateY(0);
    opacity: 1;
  }
}`
    document.head.appendChild(style)
  }, [])

  const handleStreamMessage = useCallback(
    (stream: ArrayBuffer) => {
      try {
        const ascii = String.fromCharCode(...new Uint8Array(stream))
        const [message_id, partIndexStr, totalPartsStr, content] =
          ascii.split("|")
        const part_index = parseInt(partIndexStr, 10)
        const total_parts =
          totalPartsStr === "???" ? -1 : parseInt(totalPartsStr, 10)
        if (Number.isNaN(part_index) || Number.isNaN(total_parts)) {
          return
        }
        if (total_parts === -1) {
          return
        }

        const chunk: TextChunk = {
          message_id,
          part_index,
          total_parts,
          content,
        }
        const cache = cacheRef.current
        if (!cache[message_id]) {
          cache[message_id] = []
        }
        cache[message_id].push(chunk)

        if (cache[message_id].length === total_parts) {
          const payloadRaw = reconstructMessage(cache[message_id])
          const payload = JSON.parse(base64ToUtf8(payloadRaw))
          const { text, is_final, text_ts, role } = payload
          if (text && String(text).trim().length > 0) {
            const parsed = extractSpeaker(text)
            appendOrUpdateItem({
              id: message_id,
              text: parsed.text,
              speaker: parsed.speaker,
              isFinal: !!is_final,
              role: role === "user" ? "user" : "assistant",
              ts: text_ts || Date.now(),
            })
          }
          delete cache[message_id]
        }
      } catch (e) {
        console.warn("[UI] Failed to parse stream-message", e)
      }
    },
    [appendOrUpdateItem],
  )

  const join = useCallback(async () => {
    if (joined || pending) return
    setPending(true)
    try {
      setError(null)
      const { ok, code, data, msg } = await apiGenAgoraData({
        channel,
        userId,
      })
      if (!ok || !data) {
        throw new Error(`Token error: ${String(msg)} (code=${String(code)})`)
      }

      const { default: AgoraRTC } = await import("agora-rtc-sdk-ng")
      const client = AgoraRTC.createClient({ mode: "rtc", codec: "vp8" })
      clientRef.current = client

      client.on("stream-message", (_uid: UID, stream: ArrayBuffer) => {
        handleStreamMessage(stream)
      })

      client.on("user-published", async (user, mediaType) => {
        if (mediaType !== "audio") return
        await client.subscribe(user, mediaType)
        const track = user.audioTrack
        if (track) {
          track.play()
          remoteTracksRef.current.set(String(user.uid), track)
        }
      })

      client.on("user-unpublished", (user) => {
        const track = remoteTracksRef.current.get(String(user.uid))
        track?.stop()
        remoteTracksRef.current.delete(String(user.uid))
      })

      client.on("user-left", (user) => {
        const track = remoteTracksRef.current.get(String(user.uid))
        track?.stop()
        remoteTracksRef.current.delete(String(user.uid))
      })

      await client.join(data.appId, channel, data.token, userId)

      const micTrack = await AgoraRTC.createMicrophoneAudioTrack()
      audioRef.current = micTrack
      await client.publish([micTrack])
      setMicEnabled(true)
      setJoined(true)
    } catch (err: any) {
      console.error("[UI] join error", err)
      setError(err?.message || String(err))
      await stop() // ensure state is clean
      throw err
    } finally {
      setPending(false)
    }
  }, [channel, userId, joined, pending, handleStreamMessage])

  const start = useCallback(async () => {
    if (joined || pending) return
    setPending(true)
    try {
      const { ok, msg } = await apiStartService({
        channel,
        userId,
        graphName: "diarization_demo",
      })
      if (!ok) {
        throw new Error(msg || "Failed to start agent")
      }
      await join()
    } catch (err: any) {
      console.error("[UI] start error", err)
      setError(err?.message || "Unable to start session")
      setPending(false)
    } finally {
      setPending(false)
    }
  }, [channel, userId, join, joined, pending])

  const stop = useCallback(async () => {
    setPending(true)
    try {
      cacheRef.current = {}
      setItems([])
      setLastReplySpeaker(null)
      if (audioRef.current) {
        try {
          await audioRef.current.setEnabled(false)
        } catch { }
        audioRef.current.close()
        audioRef.current = null
      }
      remoteTracksRef.current.forEach((track) => track.stop())
      remoteTracksRef.current.clear()
      if (clientRef.current) {
        try {
          await clientRef.current.leave()
        } catch { }
        clientRef.current.removeAllListeners()
        clientRef.current = null
      }
      await apiStopService(channel)
    } catch (err: any) {
      console.warn("[UI] stop error", err)
    } finally {
      setJoined(false)
      setMicEnabled(false)
      setActiveSpeaker(null)
      setPending(false)
    }
  }, [channel])

  useEffect(() => {
    setMounted(true)
    if (!userId) {
      const saved = Number(localStorage.getItem("diarization_uid") || "0")
      const id = saved || generateUserId()
      setUserId(id)
      localStorage.setItem("diarization_uid", String(id))
    }
  }, [userId])

  useEffect(() => {
    if (!joined || items.length === 0) return
    const container = transcriptContainerRef.current
    if (!container) return
    container.scrollTo({
      top: container.scrollHeight,
      behavior: "smooth",
    })
  }, [items, joined])

  useEffect(() => {
    if (!joined) {
      return
    }

    let cancelled = false
    let pingInFlight = false

    const sendPing = async () => {
      if (pingInFlight || cancelled) {
        return
      }
      pingInFlight = true
      try {
        await apiPing(channel)
      } catch (err) {
        console.warn("[UI] ping error", err)
      } finally {
        pingInFlight = false
      }
    }

    void sendPing()
    const timer = setInterval(() => {
      void sendPing()
    }, 20000)

    return () => {
      cancelled = true
      clearInterval(timer)
    }
  }, [joined, channel])

  useEffect(() => {
    return () => {
      if (audioRef.current) {
        audioRef.current.close()
      }
      remoteTracksRef.current.forEach((track) => track.stop())
      remoteTracksRef.current.clear()
      if (clientRef.current) {
        clientRef.current.leave().catch(() => { })
        clientRef.current.removeAllListeners()
        clientRef.current = null
      }
    }
  }, [])

  if (!mounted) {
    return null
  }

  return (
    <div
      style={{
        position: "relative",
        minHeight: "100vh",
        background: PAGE_BACKGROUND,
        padding: "56px 24px 72px",
        fontFamily: "Inter, system-ui, Arial",
        overflow: "hidden",
      }}
    >
      <Threads
        color={[0.85, 0.89, 0.96]}
        amplitude={0.6}
        distance={0.4}
        enableMouseInteraction={false}
        style={{
          position: "absolute",
          top: "50%",
          left: "50%",
          width: "140vw",
          height: "140vh",
          transform: "translate(-50%, -50%)",
          pointerEvents: "none",
          opacity: 0.3,
        }}
      />
      <div
        style={{
          position: "relative",
          zIndex: 1,
          maxWidth: 1120,
          margin: "0 auto",
          display: "flex",
          flexDirection: "column",
          gap: 28,
        }}
      >
        {error && (
          <div style={errorBannerStyle}>
            <span style={{ fontWeight: 600 }}>Connection issue:</span>
            <span>{error}</span>
          </div>
        )}

        <h1
          style={{
            fontSize: 34,
            fontWeight: 700,
            letterSpacing: 0.4,
            margin: 0,
            color: TEXT_PRIMARY,
          }}
        >
          Who Likes What
        </h1>

        <section
          style={{
            ...panelBaseStyle,
            padding: "36px 36px 32px",
            display: "flex",
            flexDirection: "column",
            gap: 20,
            minHeight: 560,
          }}
        >
          <div
            style={{
              display: "flex",
              alignItems: "center",
              justifyContent: "space-between",
              gap: 16,
              flexWrap: "wrap",
              color: TEXT_PRIMARY,
            }}
          >
            <div
              style={{
                display: "flex",
                flexDirection: "column",
                gap: 10,
              }}
            >
              <div
                style={{
                  display: "flex",
                  flexDirection: "column",
                  gap: 4,
                }}
              >
                <h2
                  style={{
                    ...sectionTitleStyle,
                    letterSpacing: 0.2,
                    marginBottom: 0,
                  }}
                >
                  Speaker Spotlight
                </h2>
              </div>
              <span
                style={{
                  fontSize: 12,
                  letterSpacing: 0.5,
                  color: TEXT_MUTED,
                  maxWidth: 360,
                }}
              >
                Active speaker centers automatically; cards capture the latest lines.
              </span>
            </div>
            <div
              style={{
                display: "flex",
                alignItems: "center",
                gap: 10,
              }}
            >
              {!joined ? (
                <button
                  onClick={start}
                  disabled={!userId || !channel || pending}
                  style={primaryButtonStyle(!userId || !channel || pending)}
                >
                  {pending ? "Connecting…" : "Start"}
                </button>
              ) : (
                <button onClick={stop} style={dangerButtonStyle(pending)}>
                  {pending ? "Stopping…" : "Disconnect"}
                </button>
              )}
            </div>
          </div>
          <div
            key={focusSpeaker || "default"}
            style={{
              display: "grid",
              gridTemplateColumns: "repeat(3, minmax(0, 1fr))",
              columnGap: 24,
              width: "100%",
              maxWidth: 960,
              padding: "0 16px 28px",
              margin: "0 auto",
              alignItems: "stretch",
              animation: "spotlight-slide-in 0.95s cubic-bezier(0.16, 0.72, 0.24, 1)",
            }}
          >
            {speakerCards.map((card) => (
              <div
                key={card.name}
                style={{
                  display: "flex",
                  justifyContent: "center",
                  transition: "transform 0.35s cubic-bezier(0.4, 0, 0.2, 1)",
                  transform: card.status === "active" ? "translateY(-8px)" : "translateY(0)",
                }}
                ref={(el) => {
                  if (el) {
                    cardRefs.current[card.name] = el
                  } else {
                    delete cardRefs.current[card.name]
                  }
                }}
              >
                <div
                  style={{
                    width: "100%",
                    maxWidth: 320,
                    borderRadius: 16,
                    border: card.status === "active" ? "2px solid #1f2937" : "1px solid #e5e7eb",
                    background: "#ffffff",
                    boxShadow:
                      card.status === "active"
                        ? "0 32px 56px rgba(15, 23, 42, 0.18)"
                        : "0 12px 24px rgba(15, 23, 42, 0.08)",
                    padding: "26px 24px",
                    display: "flex",
                    flexDirection: "column",
                    gap: 16,
                    minHeight: 420,
                    transformOrigin: "center",
                    transform: card.status === "active" ? "scale(1.02)" : "scale(1)",
                    transition: "transform 0.35s cubic-bezier(0.4, 0, 0.2, 1), box-shadow 0.3s ease, border 0.3s ease",
                  }}
                >
                  <div
                    style={{
                      display: "flex",
                      alignItems: "center",
                      justifyContent: "space-between",
                      gap: 12,
                    }}
                  >
                    <h3
                      style={{
                        margin: 0,
                        fontSize: 18,
                        fontWeight: 600,
                        color: TEXT_PRIMARY,
                      }}
                    >
                      {card.name}
                    </h3>
                    <span
                      style={{
                        fontSize: 11,
                        letterSpacing: 0.4,
                        textTransform: "uppercase",
                        color: card.status === "active" ? "#111827" : "#6b7280",
                      }}
                    >
                      {card.status === "active"
                        ? "Active"
                        : card.status === "idle"
                          ? "Idle"
                          : "Waiting"}
                    </span>
                  </div>
                  <div
                    style={{
                      display: "flex",
                      alignItems: "center",
                      gap: 8,
                    }}
                  >
                    <span
                      style={{
                        width: 10,
                        height: 10,
                        borderRadius: "50%",
                        background:
                          card.status === "active" ? "#22c55e" : card.accent,
                        border:
                          card.status === "active"
                            ? "1px solid rgba(34, 197, 94, 0.4)"
                            : "1px solid #d1d5db",
                        boxShadow:
                          card.status === "active"
                            ? "0 0 8px rgba(34, 197, 94, 0.45)"
                            : "none",
                      }}
                    />
                    <span
                      style={{
                        fontSize: 13,
                        color: TEXT_MUTED,
                      }}
                    >
                      {card.tagline}
                    </span>
                  </div>
                  <div
                    style={{
                      display: "flex",
                      flexDirection: "column",
                      gap: 8,
                      flexGrow: 1,
                    }}
                  >
                    {card.transcriptLines.map((line, idx) => (
                      <div
                        key={`${card.name}-line-${idx}`}
                        style={{
                          fontSize: 14,
                          lineHeight: 1.4,
                          color: TEXT_PRIMARY,
                          background: "#f9fafb",
                          borderRadius: 8,
                          padding: "12px 14px",
                        }}
                      >
                        {line}
                      </div>
                    ))}
                  </div>
                </div>
              </div>
            ))}
          </div>
        </section>

        <section
          style={{
            ...panelBaseStyle,
            display: "flex",
            flexDirection: "column",
            gap: 18,
          }}
        >
          <div
            style={{
              display: "flex",
              justifyContent: "space-between",
              alignItems: "baseline",
              flexWrap: "wrap",
              gap: 12,
            }}
          >
            <h2 style={sectionTitleStyle}>Transcript</h2>
            <span style={{ fontSize: 12, color: TEXT_MUTED }}>
              Speechmatics ASR + assistant turns
            </span>
          </div>

          <div
            ref={transcriptContainerRef}
            style={{
              maxHeight: 520,
              overflowY: "auto",
              border: `1px solid ${transcriptBorderColor}`,
              borderRadius: 16,
              background: "#ffffff",
              boxShadow: "inset 0 1px 0 rgba(15, 23, 42, 0.02)",
              padding: "18px 20px",
            }}
          >
            {items.length === 0 && (
              <div style={{ color: TEXT_MUTED, fontSize: 14 }}>
                Say hello to start the enrollment flow for Elliot, Taytay, and
                Musk.
              </div>
            )}

            {items.map((item) => (
              <TranscriptRow key={item.id} item={item} />
            ))}
          </div>
        </section>
      </div>
    </div>
  )
}

function extractSpeaker(text: string): { speaker?: string; text: string } {
  const match = text.match(SPEAKER_REGEX)
  if (match?.[1]) {
    return { speaker: match[1], text: text.slice(match[0].length) }
  }
  return { text }
}

function TranscriptRow({ item }: { item: ChatItem }) {
  const timestamp = new Date(item.ts || Date.now()).toLocaleTimeString([], {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
  })
  const isAssistant = item.role === "assistant"
  const accent = isAssistant ? "#111827" : "#4b5563"
  return (
    <div
      style={{
        padding: "14px 0",
        borderBottom: `1px solid ${transcriptBorderColor}`,
        display: "flex",
        flexDirection: "column",
        gap: 6,
      }}
    >
      <div
        style={{
          display: "flex",
          gap: 12,
          alignItems: "baseline",
          flexWrap: "wrap",
        }}
      >
        <span
          style={{
            fontSize: 11,
            letterSpacing: 0.4,
            color: TEXT_MUTED,
            minWidth: 70,
          }}
        >
          {timestamp}
        </span>
        <span
          style={{
            fontSize: 13,
            fontWeight: 600,
            color: accent,
            textTransform: "uppercase",
            letterSpacing: 0.45,
          }}
        >
          {item.speaker
            ? `${isAssistant ? "AI →" : "User"} ${item.speaker}`
            : isAssistant
              ? "AI"
              : "User"}
        </span>
        {!item.isFinal && (
          <span style={{ fontSize: 11, color: "#9ca3af" }}>…listening</span>
        )}
      </div>
      <div
        style={{
          marginLeft: 0,
          color: TEXT_PRIMARY,
          lineHeight: 1.5,
        }}
      >
        {item.text}
      </div>
    </div>
  )
}

function StatusCard({
  title,
  value,
  tone = "neutral",
}: {
  title: string
  value: string
  tone?: StatusTone
}) {
  const toneStyles: Record<
    StatusTone,
    { background: string; border: string; label: string; value: string }
  > = {
    positive: {
      background: "#ffffff",
      border: "#d1d5db",
      label: "#4b5563",
      value: "#111827",
    },
    neutral: {
      background: "#f8fafc",
      border: "#d1d5db",
      label: "#4b5563",
      value: "#111827",
    },
    warning: {
      background: "#f9fafb",
      border: "#e5e7eb",
      label: "#6b7280",
      value: "#111827",
    },
    muted: {
      background: "#f9fafb",
      border: "#e5e7eb",
      label: "#9ca3af",
      value: "#6b7280",
    },
  }

  const styles = toneStyles[tone]
  return (
    <div
      style={{
        flex: "1 1 200px",
        minWidth: 200,
        background: styles.background,
        border: `1px solid ${styles.border}`,
        borderRadius: 16,
        padding: "18px 20px",
        display: "flex",
        flexDirection: "column",
        gap: 8,
      }}
    >
      <span
        style={{
          fontSize: 11,
          fontWeight: 600,
          letterSpacing: 0.4,
          color: styles.label,
        }}
      >
        {title.toUpperCase()}
      </span>
      <span
        style={{
          fontSize: 20,
          fontWeight: 600,
          color: styles.value,
        }}
      >
        {value}
      </span>
    </div>
  )
}

function primaryButtonStyle(disabled: boolean): CSSProperties {
  return {
    padding: "12px 20px",
    borderRadius: 12,
    border: "none",
    background: disabled ? "#e5e7eb" : "#111827",
    color: disabled ? "#6b7280" : "#ffffff",
    fontWeight: 600,
    letterSpacing: 0.2,
    cursor: disabled ? "not-allowed" : "pointer",
    minWidth: 160,
    transition: "background 0.2s ease, color 0.2s ease",
  }
}

function dangerButtonStyle(disabled: boolean): CSSProperties {
  return {
    padding: "12px 20px",
    borderRadius: 12,
    border: `1px solid ${disabled ? "#e5e7eb" : "#111827"}`,
    background: disabled ? "#f9fafb" : "#ffffff",
    color: disabled ? "#9ca3af" : "#111827",
    fontWeight: 600,
    letterSpacing: 0.2,
    cursor: disabled ? "not-allowed" : "pointer",
    minWidth: 160,
    transition: "background 0.2s ease, color 0.2s ease",
  }
}

function reconstructMessage(chunks: TextChunk[]): string {
  const ordered = [...chunks].sort((a, b) => a.part_index - b.part_index)
  return ordered.map((chunk) => chunk.content).join("")
}

function base64ToUtf8(base64: string): string {
  const binaryString = atob(base64)
  const bytes = new Uint8Array(binaryString.length)
  for (let i = 0; i < binaryString.length; i++) {
    bytes[i] = binaryString.charCodeAt(i)
  }
  return new TextDecoder("utf-8").decode(bytes)
}
