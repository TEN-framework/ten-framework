import type { Metadata, Viewport } from 'next'

export const metadata: Metadata = {
  title: 'Speechmatics Diarization',
}

export const viewport: Viewport = {
  width: 'device-width',
  initialScale: 1,
}

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en" suppressHydrationWarning>
      <body style={{ margin: 0 }}>{children}</body>
    </html>
  )
}
