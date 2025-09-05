"use client"

import * as React from "react"
import clsx from "clsx"
import { Waveform } from "../components/Waveform"

export default function Page() {
  const [recording, setRecording] = React.useState(false)
  const [mediaStream, setMediaStream] = React.useState<MediaStream | null>(null)
  const [audioBlob, setAudioBlob] = React.useState<Blob | null>(null)
  const [rawText, setRawText] = React.useState("")
  const [corrected, setCorrected] = React.useState("")
  const [busy, setBusy] = React.useState(false)

  const mediaRecorderRef = React.useRef<MediaRecorder | null>(null)
  const chunksRef = React.useRef<BlobPart[]>([])

  const toggleRecording = async () => {
    if (!recording) {
      setRawText("")
      setCorrected("")
      chunksRef.current = []
      const stream = await navigator.mediaDevices.getUserMedia({ audio: true })
      setMediaStream(stream)
      const mr = new MediaRecorder(stream, { mimeType: "audio/webm" })
      mediaRecorderRef.current = mr
      mr.ondataavailable = (e) => {
        if (e.data && e.data.size > 0) chunksRef.current.push(e.data)
      }
      mr.onstop = () => {
        const blob = new Blob(chunksRef.current, { type: "audio/webm" })
        setAudioBlob(blob)
        stream.getTracks().forEach((t) => t.stop())
        setMediaStream(null)
      }
      mr.start(200)
      setRecording(true)
    } else {
      mediaRecorderRef.current?.stop()
      setRecording(false)
    }
  }

  const runTranscription = async () => {
    if (!audioBlob) return
    setBusy(true)
    try {
      // STT
      const sttResp = await fetch("/api/stt", {
        method: "POST",
        headers: { "content-type": "audio/webm" },
        body: audioBlob,
      })
      const sttJson = await sttResp.json()
      setRawText(sttJson.text || "")
      // LLM correction
      const llmResp = await fetch("/api/llm", {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ text: sttJson.text || "" }),
      })
      const llmJson = await llmResp.json()
      setCorrected(llmJson.corrected || "")
    } finally {
      setBusy(false)
    }
  }

  return (
    <main className="mx-auto flex min-h-screen max-w-2xl flex-col gap-6 p-6">
      <header className="pt-4">
        <h1 className="text-2xl font-semibold">Transcription</h1>
        <p className="text-sm text-muted-foreground">Record • Transcribe • Correct</p>
      </header>

      <section className="rounded-lg border border-border p-4">
        <div className="flex items-center justify-between">
          <button
            onClick={toggleRecording}
            className={clsx(
              "h-16 w-16 rounded-full border text-white transition",
              recording ? "bg-red-600 border-red-700" : "bg-blue-600 border-blue-700",
            )}
            title={recording ? "Stop" : "Record"}
          >
            {recording ? "Stop" : "Rec"}
          </button>
          <div className="flex-1 pl-4">
            <Waveform stream={mediaStream} active={recording} />
          </div>
        </div>

        <div className="mt-4 flex items-center gap-2">
          <button
            onClick={runTranscription}
            disabled={!audioBlob || busy}
            className={clsx(
              "rounded-md border px-3 py-2 text-sm",
              !audioBlob || busy ? "opacity-50" : "hover:bg-muted",
            )}
          >
            {busy ? "Processing..." : "Send to Model"}
          </button>
          {audioBlob && <span className="text-xs text-muted-foreground">Audio ready ({Math.round(audioBlob.size / 1024)} KB)</span>}
        </div>
      </section>

      <section className="grid gap-4 md:grid-cols-2">
        <div className="rounded-lg border border-border p-4">
          <h2 className="mb-2 text-sm font-medium text-muted-foreground">Raw Transcript</h2>
          <p className="whitespace-pre-wrap text-sm">{rawText || "—"}</p>
        </div>
        <div className="rounded-lg border border-border p-4">
          <h2 className="mb-2 text-sm font-medium text-muted-foreground">Corrected</h2>
          <p className="whitespace-pre-wrap text-sm">{corrected || "—"}</p>
        </div>
      </section>
    </main>
  )
}

