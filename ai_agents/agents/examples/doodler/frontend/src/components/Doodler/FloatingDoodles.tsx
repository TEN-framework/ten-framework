"use client";

import * as React from "react";
import { motion } from "framer-motion";
import { cn } from "@/lib/utils";

type DoodleItem = {
  key: string;
  left: string;
  top: string;
  size: number;
  rotate: number;
  opacity: number;
  color: string;
  variant:
    | "star"
    | "cloud"
    | "spark"
    | "swirl"
    | "heart"
    | "bolt"
    | "sun"
    | "flower"
    | "arrow"
    | "gift"
    | "moon"
    | "leaf";
  duration: number;
  delay: number;
};

function Star(props: { color: string }) {
  const { color } = props;
  return (
    <svg viewBox="0 0 64 64" fill="none" aria-hidden>
      <path
        d="M32 6l6.4 17.6L56 30l-17.6 6.4L32 54l-6.4-17.6L8 30l17.6-6.4L32 6z"
        stroke={color}
        strokeWidth="3"
        strokeLinejoin="round"
      />
    </svg>
  );
}

function Cloud(props: { color: string }) {
  const { color } = props;
  return (
    <svg viewBox="0 0 64 64" fill="none" aria-hidden>
      <path
        d="M18 44h29a11 11 0 0 0 0-22 14 14 0 0 0-27-2A10 10 0 0 0 18 44z"
        stroke={color}
        strokeWidth="3"
        strokeLinecap="round"
        strokeLinejoin="round"
      />
    </svg>
  );
}

function Spark(props: { color: string }) {
  const { color } = props;
  return (
    <svg viewBox="0 0 64 64" fill="none" aria-hidden>
      <path
        d="M32 10v16M32 38v16M10 32h16M38 32h16"
        stroke={color}
        strokeWidth="3"
        strokeLinecap="round"
      />
      <path
        d="M18 18l8 8M38 38l8 8M46 18l-8 8M26 38l-8 8"
        stroke={color}
        strokeWidth="3"
        strokeLinecap="round"
        opacity="0.9"
      />
    </svg>
  );
}

function Swirl(props: { color: string }) {
  const { color } = props;
  return (
    <svg viewBox="0 0 64 64" fill="none" aria-hidden>
      <path
        d="M50 20c-4-7-16-10-25-4-10 7-9 19 2 24 9 4 16-1 16-7 0-6-7-9-12-6-5 3-4 10 2 12"
        stroke={color}
        strokeWidth="3"
        strokeLinecap="round"
      />
    </svg>
  );
}

function Heart(props: { color: string }) {
  const { color } = props;
  return (
    <svg viewBox="0 0 64 64" fill="none" aria-hidden>
      <path
        d="M32 52S12 40 12 26a10 10 0 0 1 18-6 10 10 0 0 1 22 6c0 14-20 26-20 26z"
        stroke={color}
        strokeWidth="3"
        strokeLinejoin="round"
      />
    </svg>
  );
}

function Bolt(props: { color: string }) {
  const { color } = props;
  return (
    <svg viewBox="0 0 64 64" fill="none" aria-hidden>
      <path
        d="M34 6L14 34h16l-4 24 24-34H34l0-18z"
        stroke={color}
        strokeWidth="3"
        strokeLinejoin="round"
      />
    </svg>
  );
}

function Sun(props: { color: string }) {
  const { color } = props;
  return (
    <svg viewBox="0 0 64 64" fill="none" aria-hidden>
      <circle cx="32" cy="32" r="12" stroke={color} strokeWidth="3" />
      <path
        d="M32 6v8M32 50v8M6 32h8M50 32h8M14 14l6 6M44 44l6 6M50 14l-6 6M14 50l6-6"
        stroke={color}
        strokeWidth="3"
        strokeLinecap="round"
      />
    </svg>
  );
}

function Flower(props: { color: string }) {
  const { color } = props;
  return (
    <svg viewBox="0 0 64 64" fill="none" aria-hidden>
      <circle cx="32" cy="18" r="6" stroke={color} strokeWidth="3" />
      <circle cx="46" cy="28" r="6" stroke={color} strokeWidth="3" />
      <circle cx="40" cy="44" r="6" stroke={color} strokeWidth="3" />
      <circle cx="24" cy="44" r="6" stroke={color} strokeWidth="3" />
      <circle cx="18" cy="28" r="6" stroke={color} strokeWidth="3" />
      <circle cx="32" cy="32" r="4" stroke={color} strokeWidth="3" />
    </svg>
  );
}

function Arrow(props: { color: string }) {
  const { color } = props;
  return (
    <svg viewBox="0 0 64 64" fill="none" aria-hidden>
      <path
        d="M10 38c10-14 26-14 36-6"
        stroke={color}
        strokeWidth="3"
        strokeLinecap="round"
      />
      <path
        d="M42 26l10 2-6 8"
        stroke={color}
        strokeWidth="3"
        strokeLinecap="round"
        strokeLinejoin="round"
      />
    </svg>
  );
}

function Gift(props: { color: string }) {
  const { color } = props;
  return (
    <svg viewBox="0 0 64 64" fill="none" aria-hidden>
      <rect
        x="14"
        y="22"
        width="36"
        height="28"
        rx="4"
        stroke={color}
        strokeWidth="3"
      />
      <path
        d="M32 18v32M14 34h36"
        stroke={color}
        strokeWidth="3"
        strokeLinecap="round"
      />
      <path
        d="M24 18c0-4 6-6 8 0M40 18c0-4-6-6-8 0"
        stroke={color}
        strokeWidth="3"
        strokeLinecap="round"
      />
    </svg>
  );
}

function Moon(props: { color: string }) {
  const { color } = props;
  return (
    <svg viewBox="0 0 64 64" fill="none" aria-hidden>
      <path
        d="M40 10c-10 2-17 12-15 22 2 10 11 17 22 16-5 6-13 10-22 8C13 54 6 44 8 32 10 20 22 10 40 10z"
        stroke={color}
        strokeWidth="3"
        strokeLinejoin="round"
      />
    </svg>
  );
}

function Leaf(props: { color: string }) {
  const { color } = props;
  return (
    <svg viewBox="0 0 64 64" fill="none" aria-hidden>
      <path
        d="M14 38c16-16 28-12 36-4-6 14-18 24-36 18 0-6 2-10 6-14"
        stroke={color}
        strokeWidth="3"
        strokeLinecap="round"
        strokeLinejoin="round"
      />
      <path
        d="M26 44c4-6 10-10 18-12"
        stroke={color}
        strokeWidth="3"
        strokeLinecap="round"
      />
    </svg>
  );
}

function DoodleSvg(props: { variant: DoodleItem["variant"]; color: string }) {
  const { variant, color } = props;
  switch (variant) {
    case "star":
      return <Star color={color} />;
    case "cloud":
      return <Cloud color={color} />;
    case "spark":
      return <Spark color={color} />;
    case "heart":
      return <Heart color={color} />;
    case "bolt":
      return <Bolt color={color} />;
    case "sun":
      return <Sun color={color} />;
    case "flower":
      return <Flower color={color} />;
    case "arrow":
      return <Arrow color={color} />;
    case "gift":
      return <Gift color={color} />;
    case "moon":
      return <Moon color={color} />;
    case "leaf":
      return <Leaf color={color} />;
    default:
      return <Swirl color={color} />;
  }
}

const INK = "rgba(18, 18, 18, 0.7)";
const INK_SOFT = "rgba(18, 18, 18, 0.55)";

const DEFAULT_DOODLES: DoodleItem[] = [
  {
    key: "star-1",
    left: "10%",
    top: "18%",
    size: 64,
    rotate: -12,
    opacity: 0.32,
    color: INK,
    variant: "star",
    duration: 18,
    delay: 0,
  },
  {
    key: "cloud-1",
    left: "22%",
    top: "66%",
    size: 92,
    rotate: 6,
    opacity: 0.24,
    color: INK_SOFT,
    variant: "cloud",
    duration: 22,
    delay: 1.2,
  },
  {
    key: "swirl-1",
    left: "74%",
    top: "24%",
    size: 90,
    rotate: 10,
    opacity: 0.26,
    color: INK_SOFT,
    variant: "swirl",
    duration: 20,
    delay: 0.6,
  },
  {
    key: "spark-1",
    left: "86%",
    top: "62%",
    size: 58,
    rotate: 18,
    opacity: 0.3,
    color: INK,
    variant: "spark",
    duration: 16,
    delay: 1.8,
  },
  {
    key: "heart-1",
    left: "44%",
    top: "14%",
    size: 54,
    rotate: -6,
    opacity: 0.24,
    color: INK_SOFT,
    variant: "heart",
    duration: 19,
    delay: 0.3,
  },
  {
    key: "bolt-1",
    left: "56%",
    top: "78%",
    size: 64,
    rotate: -18,
    opacity: 0.28,
    color: INK,
    variant: "bolt",
    duration: 24,
    delay: 2.2,
  },
  {
    key: "spark-2",
    left: "6%",
    top: "44%",
    size: 52,
    rotate: 8,
    opacity: 0.26,
    color: INK_SOFT,
    variant: "spark",
    duration: 17,
    delay: 0.9,
  },
  {
    key: "cloud-2",
    left: "78%",
    top: "6%",
    size: 82,
    rotate: -8,
    opacity: 0.2,
    color: INK_SOFT,
    variant: "cloud",
    duration: 21,
    delay: 1.5,
  },
  {
    key: "star-2",
    left: "64%",
    top: "44%",
    size: 50,
    rotate: 16,
    opacity: 0.28,
    color: INK,
    variant: "star",
    duration: 14,
    delay: 0.4,
  },
  {
    key: "heart-2",
    left: "34%",
    top: "78%",
    size: 48,
    rotate: 12,
    opacity: 0.24,
    color: INK_SOFT,
    variant: "heart",
    duration: 18,
    delay: 2.4,
  },
  {
    key: "swirl-2",
    left: "30%",
    top: "32%",
    size: 72,
    rotate: -14,
    opacity: 0.22,
    color: INK_SOFT,
    variant: "swirl",
    duration: 23,
    delay: 1.1,
  },
  {
    key: "bolt-2",
    left: "90%",
    top: "38%",
    size: 46,
    rotate: 22,
    opacity: 0.22,
    color: INK,
    variant: "bolt",
    duration: 15,
    delay: 2.8,
  },
  {
    key: "sun-1",
    left: "6%",
    top: "6%",
    size: 72,
    rotate: 8,
    opacity: 0.26,
    color: INK,
    variant: "sun",
    duration: 20,
    delay: 0.7,
  },
  {
    key: "flower-1",
    left: "62%",
    top: "8%",
    size: 70,
    rotate: -6,
    opacity: 0.22,
    color: INK_SOFT,
    variant: "flower",
    duration: 22,
    delay: 1.9,
  },
  {
    key: "arrow-1",
    left: "18%",
    top: "52%",
    size: 80,
    rotate: 6,
    opacity: 0.24,
    color: INK,
    variant: "arrow",
    duration: 19,
    delay: 2.6,
  },
  {
    key: "gift-1",
    left: "86%",
    top: "20%",
    size: 62,
    rotate: 12,
    opacity: 0.22,
    color: INK_SOFT,
    variant: "gift",
    duration: 18,
    delay: 1.3,
  },
  {
    key: "moon-1",
    left: "8%",
    top: "80%",
    size: 70,
    rotate: -8,
    opacity: 0.24,
    color: INK,
    variant: "moon",
    duration: 21,
    delay: 0.8,
  },
  {
    key: "leaf-1",
    left: "72%",
    top: "70%",
    size: 76,
    rotate: 14,
    opacity: 0.2,
    color: INK_SOFT,
    variant: "leaf",
    duration: 24,
    delay: 2.2,
  },
  {
    key: "sun-2",
    left: "84%",
    top: "78%",
    size: 56,
    rotate: -6,
    opacity: 0.2,
    color: INK_SOFT,
    variant: "sun",
    duration: 18,
    delay: 1.6,
  },
  {
    key: "flower-2",
    left: "16%",
    top: "30%",
    size: 60,
    rotate: 10,
    opacity: 0.22,
    color: INK,
    variant: "flower",
    duration: 19,
    delay: 2.1,
  },
  {
    key: "arrow-2",
    left: "40%",
    top: "6%",
    size: 78,
    rotate: -12,
    opacity: 0.24,
    color: INK,
    variant: "arrow",
    duration: 20,
    delay: 0.5,
  },
  {
    key: "gift-2",
    left: "6%",
    top: "70%",
    size: 54,
    rotate: 8,
    opacity: 0.2,
    color: INK_SOFT,
    variant: "gift",
    duration: 21,
    delay: 1.9,
  },
  {
    key: "moon-2",
    left: "46%",
    top: "88%",
    size: 60,
    rotate: 12,
    opacity: 0.2,
    color: INK_SOFT,
    variant: "moon",
    duration: 23,
    delay: 2.5,
  },
  {
    key: "leaf-2",
    left: "92%",
    top: "50%",
    size: 62,
    rotate: -10,
    opacity: 0.18,
    color: INK_SOFT,
    variant: "leaf",
    duration: 19,
    delay: 0.9,
  },
  {
    key: "spark-3",
    left: "12%",
    top: "54%",
    size: 46,
    rotate: 18,
    opacity: 0.26,
    color: INK,
    variant: "spark",
    duration: 16,
    delay: 2.7,
  },
  {
    key: "star-3",
    left: "52%",
    top: "38%",
    size: 44,
    rotate: -8,
    opacity: 0.22,
    color: INK_SOFT,
    variant: "star",
    duration: 17,
    delay: 1.4,
  },
  {
    key: "cloud-3",
    left: "28%",
    top: "10%",
    size: 70,
    rotate: 6,
    opacity: 0.18,
    color: INK_SOFT,
    variant: "cloud",
    duration: 22,
    delay: 0.8,
  },
  {
    key: "bolt-3",
    left: "68%",
    top: "54%",
    size: 52,
    rotate: 16,
    opacity: 0.24,
    color: INK,
    variant: "bolt",
    duration: 18,
    delay: 2.3,
  },
  {
    key: "heart-3",
    left: "22%",
    top: "86%",
    size: 44,
    rotate: -12,
    opacity: 0.2,
    color: INK_SOFT,
    variant: "heart",
    duration: 19,
    delay: 2.9,
  },
  {
    key: "swirl-3",
    left: "82%",
    top: "32%",
    size: 68,
    rotate: 10,
    opacity: 0.2,
    color: INK_SOFT,
    variant: "swirl",
    duration: 24,
    delay: 1.7,
  },
];

export default function FloatingDoodles(props: { reducedMotion: boolean }) {
  const { reducedMotion } = props;

  return (
    <div aria-hidden className="absolute inset-0">
      {DEFAULT_DOODLES.map((d) => {
        const Comp = (
          <div
            className="h-full w-full"
            style={{
              opacity: d.opacity,
              filter: "blur(0.2px)",
              mixBlendMode: "multiply",
            }}
          >
            <DoodleSvg variant={d.variant} color={d.color} />
          </div>
        );

        return reducedMotion ? (
          <div
            key={d.key}
            className="absolute"
            style={{
              left: d.left,
              top: d.top,
              width: d.size,
              height: d.size,
              transform: `rotate(${d.rotate}deg)`,
            }}
          >
            {Comp}
          </div>
        ) : (
          <motion.div
            key={d.key}
            className={cn("absolute")}
            style={{
              left: d.left,
              top: d.top,
              width: d.size,
              height: d.size,
            }}
            initial={{ opacity: 0, y: 8, rotate: d.rotate }}
            animate={{
              opacity: 1,
              x: [0, 12, 0],
              y: [0, -14, 0],
              rotate: [d.rotate, d.rotate + 4, d.rotate],
            }}
            transition={{
              duration: d.duration,
              repeat: Infinity,
              ease: "easeInOut",
              delay: d.delay,
            }}
          >
            {Comp}
          </motion.div>
        );
      })}
    </div>
  );
}
