"use client"

import * as React from "react"

export function Waveform({ stream, active }: { stream: MediaStream | null; active: boolean }) {
  const canvasRef = React.useRef<HTMLCanvasElement>(null)
  const analyserRef = React.useRef<AnalyserNode | null>(null)
  const rafRef = React.useRef<number | null>(null)

  React.useEffect(() => {
    if (!active || !stream) return
    const audioCtx = new (window.AudioContext || (window as any).webkitAudioContext)()
    const source = audioCtx.createMediaStreamSource(stream)
    const analyser = audioCtx.createAnalyser()
    analyser.fftSize = 2048
    source.connect(analyser)
    analyserRef.current = analyser

    const draw = () => {
      const canvas = canvasRef.current
      const analyser = analyserRef.current
      if (!canvas || !analyser) return
      const ctx = canvas.getContext("2d")!
      const bufferLength = analyser.fftSize
      const dataArray = new Uint8Array(bufferLength)
      analyser.getByteTimeDomainData(dataArray)
      const { width, height } = canvas
      ctx.clearRect(0, 0, width, height)
      ctx.lineWidth = 2
      ctx.strokeStyle = "#2563eb" // Tailwind blue-600
      ctx.beginPath()
      const sliceWidth = width / bufferLength
      let x = 0
      for (let i = 0; i < bufferLength; i++) {
        const v = dataArray[i] / 128.0
        const y = (v * height) / 2
        if (i === 0) ctx.moveTo(x, y)
        else ctx.lineTo(x, y)
        x += sliceWidth
      }
      ctx.lineTo(width, height / 2)
      ctx.stroke()
      rafRef.current = requestAnimationFrame(draw)
    }
    draw()
    return () => {
      if (rafRef.current) cancelAnimationFrame(rafRef.current)
      audioCtx.close()
    }
  }, [stream, active])

  return <canvas ref={canvasRef} className="h-24 w-full rounded-md border border-border" />
}

