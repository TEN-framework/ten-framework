"use client";

import * as React from "react";
import { motion } from "framer-motion";
import { cn } from "@/lib/utils";

export type DoodlePhase = "idle" | "queued" | "sketch" | "color" | "complete";
export type CreativeMode = "classic" | "neon";

export default function MagicCanvasBackground(props: {
  phase: DoodlePhase;
  mode: CreativeMode;
  reducedMotion?: boolean;
}) {
  const { phase, mode, reducedMotion = false } = props;
  const isGenerating = phase === "queued" || phase === "sketch" || phase === "color";
  const showDream = phase === "complete";
  const tileStart = "0px 0px";
  const tileEnd = "160px 140px";

  return (
    <div
      aria-hidden
      className="pointer-events-none absolute inset-0 overflow-hidden"
    >
      <motion.div
        className="absolute inset-0 doodle-paper"
        initial={false}
        animate={{ scale: isGenerating ? 1.02 : 1 }}
        transition={{ duration: 1.2, ease: [0.22, 1, 0.36, 1] }}
      />
      <motion.div
        className="absolute inset-0 doodle-wash"
        initial={false}
        animate={{
          opacity: isGenerating ? 0.95 : 0.75,
          x: isGenerating ? 6 : 0,
          y: isGenerating ? -6 : 0,
        }}
        transition={{ duration: 2.4, ease: "easeInOut" }}
      />
      <motion.div
        className="absolute inset-0 doodle-tile"
        initial={false}
        animate={
          reducedMotion
            ? undefined
            : {
                backgroundPosition: [tileStart, tileEnd],
              }
        }
        transition={
          reducedMotion
            ? undefined
            : {
                duration: 32,
                repeat: Infinity,
                ease: "linear",
              }
        }
      />
      <motion.div
        className={cn(
          "absolute inset-0 doodle-dream",
          mode === "neon" ? "doodle-dream--neon" : "doodle-dream--classic"
        )}
        initial={false}
        animate={{ opacity: showDream ? 1 : 0, scale: showDream ? 1 : 1.06 }}
        transition={{ duration: 1.4, ease: [0.22, 1, 0.36, 1] }}
      />
      <div className="absolute inset-0 doodle-vignette" />
    </div>
  );
}
