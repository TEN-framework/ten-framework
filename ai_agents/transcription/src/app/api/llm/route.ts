import { NextRequest, NextResponse } from "next/server"

export const runtime = "nodejs"

export async function POST(req: NextRequest) {
  try {
    const { text } = await req.json()
    if (!text || typeof text !== "string") {
      return NextResponse.json({ error: "Missing text" }, { status: 400 })
    }

    // Placeholder: call your LLM provider to correct/proofread here
    // For now, return a simple "corrected" version (trim + capitalize first letter + period).
    const cleaned = text.trim()
    const corrected = cleaned
      ? cleaned.charAt(0).toUpperCase() + cleaned.slice(1) + (/[.!?]$/.test(cleaned) ? "" : ".")
      : ""

    return NextResponse.json({ corrected })
  } catch (e: any) {
    return NextResponse.json({ error: e?.message || "LLM correction failed" }, { status: 500 })
  }
}

