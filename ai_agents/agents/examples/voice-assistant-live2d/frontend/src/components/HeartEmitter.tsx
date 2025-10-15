'use client';

import React, { useEffect, useRef, useState } from "react";

type Heart = {
  id: number;
  baseX: number;
  jitter: number;
  verticalJitter: number;
  size: number;
  duration: number;
  rotation: number;
  side: "left" | "right";
};

interface HeartEmitterProps {
  active: boolean;
}

export function HeartEmitter({ active }: HeartEmitterProps) {
  const [hearts, setHearts] = useState<Heart[]>([]);
  const nextId = useRef(0);

  useEffect(() => {
    const spawnHeart = () => {
      if (!active) {
        return;
      }

      const id = nextId.current++;
      const side: Heart["side"] = Math.random() > 0.5 ? "left" : "right";
      const baseX =
        (side === "left" ? -1 : 1) * (90 + Math.random() * 55); // around cheeks

      const heart: Heart = {
        id,
        baseX,
        jitter: Math.random() * 34 - 17,
        verticalJitter: Math.random() * 28 - 14,
        size: 16 + Math.random() * 14,
        duration: 1.9 + Math.random() * 1.3,
        rotation: Math.random() * 36 - 18,
        side,
      };

      setHearts((prev) => [...prev, heart]);

      // Remove heart after animation
      setTimeout(() => {
        setHearts((prev) => prev.filter((h) => h.id !== id));
      }, heart.duration * 1000);
    };

    const interval = setInterval(spawnHeart, 230);
    spawnHeart();

    return () => {
      clearInterval(interval);
    };
  }, [active]);

  return (
    <div className="pointer-events-none absolute bottom-[24%] left-1/2 flex h-0 w-full max-w-[460px] -translate-x-1/2 justify-center">
      {hearts.map((heart) => (
        <span
          key={heart.id}
          className="heart-emit select-none"
          style={{
            fontSize: `${heart.size}px`,
            animationDuration: `${heart.duration}s`,
            ["--heart-base-x" as string]: `${heart.baseX}px`,
            ["--heart-jitter" as string]: `${heart.jitter}px`,
            ["--heart-y" as string]: `${heart.verticalJitter}px`,
            ["--heart-rot" as string]: `${heart.rotation}deg`,
          } as React.CSSProperties}
        >
          ❤️
        </span>
      ))}
    </div>
  );
}
