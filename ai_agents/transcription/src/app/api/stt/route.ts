import { NextRequest, NextResponse } from "next/server"

export const runtime = "nodejs"

export async function POST(req: NextRequest) {
  try {
    const contentType = req.headers.get("content-type") || ""
    if (!contentType.includes("audio/")) {
      return NextResponse.json({ error: "Expected audio/* content-type" }, { status: 400 })
    }

    // Read raw audio bytes
    const arrayBuffer = await req.arrayBuffer()
    const audioBytes = Buffer.from(arrayBuffer)

    // Placeholder: If OPENAI_API_KEY is set, you can call Whisper or other STT providers here.
    // To keep this repo network-neutral, we return a mock transcript length.
    const mockText = `Transcribed ${audioBytes.byteLength} bytes of audio (mock)`

    return NextResponse.json({ text: mockText })
  } catch (e: any) {
    return NextResponse.json({ error: e?.message || "STT failed" }, { status: 500 })
  }
}

