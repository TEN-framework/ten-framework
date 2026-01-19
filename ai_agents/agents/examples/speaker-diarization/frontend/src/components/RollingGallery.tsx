"use client"

import { useEffect, useMemo, useRef, useState } from "react"
import type { PointerEvent as ReactPointerEvent } from "react"
import "./RollingGallery.css"

export type GalleryItem = {
  title: string
  tagline?: string
  detail?: string
  accentColor?: string
  status?: "active" | "idle"
}

type RollingGalleryProps = {
  items: GalleryItem[]
  autoplay?: boolean
  pauseOnHover?: boolean
}

const DEFAULT_ITEMS: GalleryItem[] = [
  {
    title: "Speaker",
    tagline: "Awaiting Voice",
    detail: "Say something to enrol in the session.",
  },
]

export default function RollingGallery(props: RollingGalleryProps) {
  const { autoplay = false, pauseOnHover = false } = props
  const items = props.items && props.items.length > 0 ? props.items : DEFAULT_ITEMS
  const [isSmallScreen, setIsSmallScreen] = useState(false)
  const [isHovered, setIsHovered] = useState(false)
  const [rotation, setRotation] = useState(0)

  const dragState = useRef<{
    active: boolean
    originX: number
    rotationAtStart: number
  }>({ active: false, originX: 0, rotationAtStart: 0 })

  useEffect(() => {
    if (typeof window === "undefined") {
      return
    }
    const handleResize = () => {
      setIsSmallScreen(window.innerWidth <= 640)
    }
    handleResize()
    window.addEventListener("resize", handleResize)
    return () => window.removeEventListener("resize", handleResize)
  }, [])

  const cylinderWidth = isSmallScreen ? 1100 : 1600
  const faceCount = Math.max(items.length, 3)
  const faceWidth = (cylinderWidth / faceCount) * 1.35
  const dragFactor = 0.05
  const radius = cylinderWidth / (2 * Math.PI)
  const autoplayRef = useRef<NodeJS.Timeout | null>(null)

  const extendedItems = useMemo(() => {
    if (items.length >= faceCount) {
      return items
    }
    const repeats = Math.ceil(faceCount / items.length)
      return Array.from({ length: faceCount }, (_, idx) => items[idx % items.length])
  }, [items, faceCount])

  useEffect(() => {
    if (!autoplay || (pauseOnHover && isHovered)) {
      if (autoplayRef.current) {
        clearInterval(autoplayRef.current)
        autoplayRef.current = null
      }
      return
    }
    const step = () => {
      setRotation((prev) => prev - 360 / faceCount)
    }
    autoplayRef.current = setInterval(step, 2400)
    return () => {
      if (autoplayRef.current) {
        clearInterval(autoplayRef.current)
        autoplayRef.current = null
      }
    }
  }, [autoplay, pauseOnHover, isHovered, faceCount])

  const handlePointerDown = (event: ReactPointerEvent<HTMLDivElement>) => {
    const track = event.currentTarget
    track.setPointerCapture(event.pointerId)
    dragState.current = {
      active: true,
      originX: event.clientX,
      rotationAtStart: rotation,
    }
  }

  const handlePointerMove = (event: ReactPointerEvent<HTMLDivElement>) => {
    if (!dragState.current.active) return
    const delta = event.clientX - dragState.current.originX
    setRotation(dragState.current.rotationAtStart + delta * dragFactor)
  }

  const handlePointerUp = (event: ReactPointerEvent<HTMLDivElement>) => {
    if (!dragState.current.active) return
    const track = event.currentTarget
    track.releasePointerCapture(event.pointerId)
    dragState.current.active = false
  }

  return (
    <div className="gallery-container">
      <div className="gallery-gradient gallery-gradient-left" />
      <div className="gallery-gradient gallery-gradient-right" />
      <div className="gallery-content">
        <div
          className="gallery-track"
          onMouseEnter={() => setIsHovered(true)}
          onMouseLeave={() => setIsHovered(false)}
          onPointerDown={handlePointerDown}
          onPointerMove={handlePointerMove}
          onPointerUp={handlePointerUp}
          onPointerCancel={handlePointerUp}
          style={{
            transform: `rotate3d(0, 1, 0, ${rotation}deg)`,
            width: cylinderWidth,
            transformStyle: "preserve-3d",
          }}
        >
          {extendedItems.map((item, index) => (
            <div
              key={`${item.title}-${index}`}
              className="gallery-item"
              style={{
                width: `${faceWidth}px`,
                transform: `rotateY(${index * (360 / faceCount)}deg) translateZ(${radius}px)`,
              }}
            >
              <div
                className="gallery-card"
                style={{
                  borderColor:
                    item.status === "active"
                      ? "rgba(93, 217, 255, 0.9)"
                      : "rgba(255, 255, 255, 0.35)",
                  boxShadow:
                    item.status === "active"
                      ? "0 22px 48px rgba(46, 196, 255, 0.45)"
                      : "0 18px 40px rgba(20, 9, 45, 0.35)",
                }}
              >
                <div className="gallery-accent">
                  <span
                    className="gallery-accent-dot"
                    style={{ backgroundColor: item.accentColor || "#94a3b8" }}
                  />
                  <span>{item.status === "active" ? "Speaking" : "Waiting"}</span>
                </div>
                <h3>{item.title}</h3>
                {item.tagline && <p>{item.tagline}</p>}
                {item.detail && (
                  <p style={{ fontSize: "0.8rem", opacity: 0.7 }}>{item.detail}</p>
                )}
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  )
}
