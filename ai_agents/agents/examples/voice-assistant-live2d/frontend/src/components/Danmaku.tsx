"use client";

import React, { useEffect, useState, useRef } from 'react';

export interface DanmakuMessage {
    id: string;
    uid: number;
    content: string;
    timestamp: number;
}

interface DanmakuProps {
    messages: DanmakuMessage[];
    className?: string;
}

interface ActiveDanmaku extends DanmakuMessage {
    lane: number;
    animationDuration: number;
    startTime: number;
}

const Danmaku: React.FC<DanmakuProps> = ({ messages, className = '' }) => {
    const [activeDanmakus, setActiveDanmakus] = useState<ActiveDanmaku[]>([]);
    const [processedIds, setProcessedIds] = useState<Set<string>>(new Set());
    const timersRef = useRef<Map<string, NodeJS.Timeout>>(new Map());

    // Number of lanes for danmaku
    const LANE_COUNT = 5;
    const [laneTimers, setLaneTimers] = useState<number[]>(Array(LANE_COUNT).fill(0));

    useEffect(() => {
        // Process new messages
        const newMessages = messages.filter(msg => !processedIds.has(msg.id));

        if (newMessages.length === 0) return;

        const now = Date.now();
        const updatedLaneTimers = [...laneTimers];

        newMessages.forEach(msg => {
            // Find available lane (lane that hasn't been used recently)
            let availableLane = 0;
            let minTime = updatedLaneTimers[0];

            for (let i = 1; i < LANE_COUNT; i++) {
                if (updatedLaneTimers[i] < minTime) {
                    minTime = updatedLaneTimers[i];
                    availableLane = i;
                }
            }

            // Random duration between 8-12 seconds
            const duration = 8 + Math.random() * 4;

            // Update lane timer
            updatedLaneTimers[availableLane] = now + duration * 1000;

            // Add to active danmakus
            const newDanmaku: ActiveDanmaku = {
                ...msg,
                lane: availableLane,
                animationDuration: duration,
                startTime: now
            };

            setActiveDanmakus(prev => [...prev, newDanmaku]);
            setProcessedIds(prev => new Set([...prev, msg.id]));

            // Set individual timer for this danmaku
            const timer = setTimeout(() => {
                setActiveDanmakus(prev => prev.filter(d => d.id !== msg.id));
                timersRef.current.delete(msg.id);
            }, duration * 1000);

            timersRef.current.set(msg.id, timer);
        });

        setLaneTimers(updatedLaneTimers);
    }, [messages]);

    // Cleanup on unmount
    useEffect(() => {
        return () => {
            timersRef.current.forEach((timer: NodeJS.Timeout) => clearTimeout(timer));
            timersRef.current.clear();
        };
    }, []);

    return (
        <div className={`absolute inset-0 pointer-events-none overflow-hidden ${className}`}>
            {activeDanmakus.map((danmaku) => (
                <div
                    key={danmaku.id}
                    className="absolute whitespace-nowrap animate-danmaku"
                    style={{
                        top: `${(danmaku.lane / LANE_COUNT) * 100 + 10}%`,
                        right: '-100%',
                        animationDuration: `${danmaku.animationDuration}s`,
                        animationTimingFunction: 'linear',
                        animationFillMode: 'forwards',
                    }}
                >
                    <div className="inline-flex items-center gap-2 rounded-full bg-gradient-to-r from-[#ff6b9d]/50 to-[#c44569]/50 px-4 py-2 shadow-lg backdrop-blur-sm border border-white/30">
                        {/* User Avatar */}
                        <div className="flex h-6 w-6 items-center justify-center rounded-full bg-white/20 text-xs font-bold text-white">
                            {danmaku.uid.toString().slice(-2)}
                        </div>

                        {/* Message Content */}
                        <span className="text-sm font-medium text-white drop-shadow-md">
                            {danmaku.content}
                        </span>
                    </div>
                </div>
            ))}

            <style jsx>{`
        @keyframes danmaku {
          from {
            transform: translateX(0);
          }
          to {
            transform: translateX(calc(-100vw - 100%));
          }
        }

        .animate-danmaku {
          animation-name: danmaku;
        }
      `}</style>
        </div>
    );
};

export default Danmaku;
