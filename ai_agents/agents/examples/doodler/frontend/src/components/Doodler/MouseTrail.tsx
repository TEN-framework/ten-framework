"use client";

import * as React from "react";

type CreativeMode = "classic" | "neon";

type Particle = {
  x: number;
  y: number;
  vx: number;
  vy: number;
  r: number;
  life: number;
  ttl: number;
  hue: number;
};

export default function MouseTrail(props: {
  enabled: boolean;
  mode: CreativeMode;
}) {
  const { enabled, mode } = props;
  const canvasRef = React.useRef<HTMLCanvasElement | null>(null);
  const particlesRef = React.useRef<Particle[]>([]);
  const rafRef = React.useRef<number | null>(null);
  const pointerRef = React.useRef<{ x: number; y: number; t: number } | null>(
    null
  );

  React.useEffect(() => {
    if (!enabled) {
      particlesRef.current = [];
      return;
    }
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext("2d");
    if (!ctx) return;

    const dpr = () => Math.max(1, Math.min(2, window.devicePixelRatio || 1));

    const resize = () => {
      const ratio = dpr();
      canvas.width = Math.floor(window.innerWidth * ratio);
      canvas.height = Math.floor(window.innerHeight * ratio);
      canvas.style.width = `${window.innerWidth}px`;
      canvas.style.height = `${window.innerHeight}px`;
      ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
    };

    resize();
    window.addEventListener("resize", resize);

    const spawn = (x: number, y: number, speedX: number, speedY: number) => {
      const ttl = mode === "neon" ? 520 : 340;
      const r = mode === "neon" ? 2.2 : 1.6;
      const hue = mode === "neon" ? 175 + Math.random() * 90 : 32;
      particlesRef.current.push({
        x,
        y,
        vx: speedX,
        vy: speedY,
        r: r + Math.random() * 1.2,
        life: 0,
        ttl,
        hue,
      });
      if (particlesRef.current.length > 180) {
        particlesRef.current.splice(0, particlesRef.current.length - 180);
      }
    };

    const onMove = (e: PointerEvent) => {
      const now = performance.now();
      const last = pointerRef.current;
      pointerRef.current = { x: e.clientX, y: e.clientY, t: now };
      if (!last) return;
      const dt = Math.max(8, now - last.t);
      const dx = e.clientX - last.x;
      const dy = e.clientY - last.y;
      const vx = dx / dt;
      const vy = dy / dt;
      const dist = Math.hypot(dx, dy);
      const count = Math.min(6, Math.max(1, Math.floor(dist / 18)));
      for (let i = 0; i < count; i += 1) {
        const t = (i + 1) / (count + 1);
        spawn(last.x + dx * t, last.y + dy * t, vx * 10, vy * 10);
      }
    };

    window.addEventListener("pointermove", onMove, { passive: true });

    const tick = (now: number) => {
      ctx.clearRect(0, 0, canvas.width, canvas.height);

      const particles = particlesRef.current;
      ctx.save();
      ctx.globalCompositeOperation = mode === "neon" ? "lighter" : "source-over";

      for (let i = particles.length - 1; i >= 0; i -= 1) {
        const p = particles[i];
        p.life += 16;
        const a = Math.max(0, 1 - p.life / p.ttl);
        p.x += p.vx * 0.016;
        p.y += p.vy * 0.016;
        p.vx *= 0.92;
        p.vy *= 0.92;

        if (a <= 0.02) {
          particles.splice(i, 1);
          continue;
        }

        if (mode === "neon") {
          ctx.shadowBlur = 14 * a;
          ctx.shadowColor = `hsla(${p.hue}, 95%, 62%, ${0.65 * a})`;
          ctx.fillStyle = `hsla(${p.hue}, 95%, 62%, ${0.55 * a})`;
        } else {
          ctx.shadowBlur = 0;
          ctx.fillStyle = `rgba(54, 42, 28, ${0.18 * a})`;
        }

        ctx.beginPath();
        ctx.arc(p.x, p.y, p.r, 0, Math.PI * 2);
        ctx.fill();
      }

      ctx.restore();
      rafRef.current = window.requestAnimationFrame(tick);
    };

    rafRef.current = window.requestAnimationFrame(tick);

    return () => {
      window.removeEventListener("resize", resize);
      window.removeEventListener("pointermove", onMove);
      if (rafRef.current) window.cancelAnimationFrame(rafRef.current);
      rafRef.current = null;
      particlesRef.current = [];
      pointerRef.current = null;
    };
  }, [enabled, mode]);

  return (
    <canvas
      ref={canvasRef}
      aria-hidden
      className="pointer-events-none absolute inset-0 z-0"
    />
  );
}

