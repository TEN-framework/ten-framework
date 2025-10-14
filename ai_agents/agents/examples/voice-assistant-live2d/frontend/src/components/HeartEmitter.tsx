'use client';

import React, { useEffect, useRef, useState } from "react";

type Heart = {
  id: number;
  offset: number;
  size: number;
  duration: number;
  rotation: number;
};

interface HeartEmitterProps {
  active: boolean;
}

export function HeartEmitter({ active }: HeartEmitterProps) {
  const [hearts, setHearts] = useState<Heart[]>([]);
  const nextId = useRef(0);

  useEffect(() => {
    if (!active) {
      return;
    }

    const spawnHeart = () => {
      const id = nextId.current++;
      const heart: Heart = {
        id,
        offset: Math.random() * 26 - 13, // drift left/right
        size: 14 + Math.random() * 10,
        duration: 1.6 + Math.random() * 0.9,
        rotation: (Math.random() * 26 - 13) // degrees
      };

      setHearts((prev) => [...prev, heart]);

      // Remove heart after animation
      setTimeout(() => {
        setHearts((prev) => prev.filter((h) => h.id !== id));
      }, heart.duration * 1000);
    };

    // Spawn immediately for responsiveness
    spawnHeart();
    const interval = setInterval(spawnHeart, 240);

    return () => {
      clearInterval(interval);
    };
  }, [active]);

  return (
    <div className="pointer-events-none absolute bottom-[18%] left-1/2 -translate-x-1/2">
      {hearts.map((heart) => (
        <span
          key={heart.id}
          className="heart-emit select-none"
          style={{
            fontSize: `${heart.size}px`,
            animationDuration: `${heart.duration}s`,
            transformOrigin: "center",
            ["--heart-x" as string]: `${heart.offset}px`,
            ["--heart-rot" as string]: `${heart.rotation}deg`,
          } as React.CSSProperties}
        >
          ❤️
        </span>
      ))}
    </div>
  );
}
