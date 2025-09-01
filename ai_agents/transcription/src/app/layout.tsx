import type { Metadata } from "next"
import "./globals.css"

export const metadata: Metadata = {
  title: "TEN Transcription",
  description: "Record, transcribe, correct",
}

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  )
}

