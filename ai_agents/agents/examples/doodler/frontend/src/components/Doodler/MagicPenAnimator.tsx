"use client";

import * as React from "react";
import { AnimatePresence, motion } from "framer-motion";
import { cn } from "@/lib/utils";
import type { CreativeMode, DoodlePhase } from "./MagicCanvasBackground";

function PenIcon(props: {
  mode: CreativeMode;
  bodyColor?: string;
  topColor?: string;
}) {
  const { mode, bodyColor, topColor } = props;
  const isNeon = mode === "neon";
  const body = bodyColor ?? (isNeon ? "#FF8A3D" : "#F97316");
  const bodyTop = topColor ?? (isNeon ? "#FFB457" : "#FB923C");
  const stripe = isNeon ? "#0B1220" : "#1F1F1F";
  const outline = isNeon ? "rgba(124,255,250,0.4)" : "rgba(0,0,0,0.18)";
  return (
    <svg
      width="44"
      height="44"
      viewBox="0 0 64 64"
      fill="none"
      aria-hidden
    >
      <g transform="rotate(-20 32 32)">
        <rect x="18" y="10" width="28" height="40" rx="9" fill={body} stroke={outline} />
        <rect x="18" y="10" width="28" height="10" rx="5" fill={bodyTop} />
        <rect x="18" y="22" width="28" height="5" rx="2.5" fill={stripe} />
        <rect x="18" y="30" width="28" height="5" rx="2.5" fill={stripe} />
        <rect x="18" y="42" width="28" height="5" rx="2.5" fill={stripe} />
        <ellipse cx="32" cy="36" rx="6.5" ry="10" fill={stripe} />
        <path
          d="M22 18c3-4 11-4 14 0"
          stroke="rgba(255,255,255,0.45)"
          strokeWidth="3"
          strokeLinecap="round"
        />
      </g>
    </svg>
  );
}

function scribbleStroke(mode: CreativeMode, phase: DoodlePhase) {
  const isNeon = mode === "neon";
  if (phase === "sketch") {
    return {
      color: isNeon ? "hsla(180, 95%, 60%, 0.75)" : "rgba(44, 33, 22, 0.45)",
      width: isNeon ? 3.5 : 3,
      dash: isNeon ? "10 10" : "9 10",
      glow: isNeon,
    };
  }
  if (phase === "color") {
    return {
      color: isNeon ? "hsla(280, 95%, 68%, 0.75)" : "rgba(249, 115, 22, 0.35)",
      width: isNeon ? 4 : 3.4,
      dash: isNeon ? "0" : "0",
      glow: isNeon,
    };
  }
  return {
    color: isNeon ? "hsla(200, 95%, 60%, 0.7)" : "rgba(44, 33, 22, 0.35)",
    width: isNeon ? 3.2 : 3,
    dash: "10 10",
    glow: isNeon,
  };
}

export default function MagicPenAnimator(props: {
  phase: DoodlePhase;
  mode: CreativeMode;
  reducedMotion: boolean;
  showPen?: boolean;
  penBodyColor?: string;
  penTopColor?: string;
}) {
  const { phase, mode, reducedMotion, showPen = true, penBodyColor, penTopColor } =
    props;
  const show = phase !== "idle";
  const isNeon = mode === "neon";

  const penAnimation = React.useMemo(() => {
    if (phase === "queued") {
      return {
        x: ["-12%", "18%", "18%"],
        y: ["-10%", "22%", "24%"],
        rotate: [-10, 6, -3],
      };
    }
    if (phase === "sketch") {
      return {
        x: ["18%", "64%", "42%", "76%", "30%", "58%"],
        y: ["24%", "30%", "64%", "58%", "46%", "34%"],
        rotate: [8, 22, -18, 12, -10, 18],
      };
    }
    if (phase === "color") {
      return {
        x: ["58%", "38%", "70%", "52%"],
        y: ["34%", "56%", "50%", "36%"],
        rotate: [10, -6, 14, 6],
      };
    }
    return {
      x: ["52%", "66%", "110%"],
      y: ["52%", "20%", "-30%"],
      rotate: [8, 28, 48],
    };
  }, [phase]);

  const penTransition = React.useMemo(() => {
    if (phase === "queued") {
      return { duration: 0.9, ease: [0.22, 1, 0.36, 1] };
    }
    if (phase === "sketch") {
      return { duration: 1.8, ease: "easeInOut", repeat: Infinity };
    }
    if (phase === "color") {
      return { duration: 2.4, ease: "easeInOut", repeat: Infinity };
    }
    return { duration: 0.9, ease: [0.22, 1, 0.36, 1] };
  }, [phase]);

  const stroke = scribbleStroke(mode, phase);

  if (reducedMotion) return null;

  return (
    <AnimatePresence>
      {show ? (
        <motion.div
          key="magic-pen"
          className="pointer-events-none absolute inset-0 z-20"
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          exit={{ opacity: 0 }}
          transition={{ duration: 0.25 }}
        >
          <motion.svg
            className="absolute inset-0 h-full w-full"
            viewBox="0 0 1000 600"
            preserveAspectRatio="none"
          >
            {phase !== "complete" ? (
              <>
                <motion.path
                  d="M120 190 C 250 70, 360 250, 520 150 S 860 220, 900 120"
                  fill="none"
                  stroke={stroke.color}
                  strokeWidth={stroke.width}
                  strokeLinecap="round"
                  strokeDasharray={stroke.dash}
                  style={{
                    filter: stroke.glow
                      ? "drop-shadow(0 0 12px rgba(124, 255, 250, 0.35))"
                      : "none",
                  }}
                  initial={{ pathLength: 0, opacity: 0 }}
                  animate={{ pathLength: 1, opacity: 1 }}
                  transition={{
                    duration: phase === "sketch" ? 1.4 : 1.8,
                    ease: "easeInOut",
                  }}
                />
                <motion.path
                  d="M150 390 C 320 520, 420 260, 610 420 S 840 520, 920 360"
                  fill="none"
                  stroke={cn(stroke.color)}
                  strokeWidth={stroke.width}
                  strokeLinecap="round"
                  strokeDasharray={stroke.dash}
                  style={{
                    filter: stroke.glow
                      ? "drop-shadow(0 0 14px rgba(255, 122, 216, 0.25))"
                      : "none",
                  }}
                  initial={{ pathLength: 0, opacity: 0 }}
                  animate={{ pathLength: 1, opacity: phase === "sketch" ? 0.85 : 0.6 }}
                  transition={{
                    duration: phase === "sketch" ? 1.2 : 2.1,
                    ease: "easeInOut",
                    delay: 0.12,
                  }}
                />
              </>
            ) : (
              <>
                <motion.path
                  d="M260 420 C 360 500, 540 500, 620 420 C 690 350, 760 360, 810 420"
                  fill="none"
                  stroke={isNeon ? "hsla(180, 95%, 60%, 0.8)" : "rgba(44, 33, 22, 0.55)"}
                  strokeWidth={isNeon ? 4.2 : 3.4}
                  strokeLinecap="round"
                  initial={{ pathLength: 0, opacity: 0 }}
                  animate={{ pathLength: 1, opacity: 1 }}
                  transition={{ duration: 0.9, ease: "easeInOut" }}
                  style={{
                    filter: isNeon
                      ? "drop-shadow(0 0 18px rgba(124, 255, 250, 0.35))"
                      : "none",
                  }}
                />
                {Array.from({ length: 12 }).map((_, i) => (
                  <motion.circle
                    // biome-ignore lint/suspicious/noArrayIndexKey: deterministic decorative sparkles
                    key={i}
                    cx={520 + (i % 6) * 40 - 100}
                    cy={300 + Math.floor(i / 6) * 40 - 40}
                    r={isNeon ? 4 : 3}
                    fill={isNeon ? "hsla(280, 95%, 68%, 0.85)" : "rgba(249, 115, 22, 0.55)"}
                    initial={{ opacity: 0, scale: 0.6 }}
                    animate={{ opacity: [0, 1, 0], scale: [0.6, 1.2, 0.9] }}
                    transition={{ duration: 0.9, delay: i * 0.03 }}
                  />
                ))}
              </>
            )}
          </motion.svg>

          {showPen ? (
            <motion.div
              className={cn(
                "absolute left-0 top-0",
                isNeon ? "drop-shadow-[0_0_18px_rgba(124,255,250,0.35)]" : ""
              )}
              initial={{ opacity: 0 }}
              animate={{ opacity: 1, ...penAnimation }}
              transition={penTransition as any}
            >
              <PenIcon mode={mode} bodyColor={penBodyColor} topColor={penTopColor} />
            </motion.div>
          ) : null}
        </motion.div>
      ) : null}
    </AnimatePresence>
  );
}
