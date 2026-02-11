"use client";

import * as React from "react";
import { useAppSelector } from "@/common";
import { cn } from "@/lib/utils";
import { EMessageDataType, EMessageType } from "@/types";

const MAX_ITEMS = 2;

export default function AvatarTranscriptOverlay(props: { className?: string }) {
  const { className } = props;
  const chatItems = useAppSelector((state) => state.global.chatItems);
  const containerRef = React.useRef<HTMLDivElement>(null);

  const displayItems = React.useMemo(() => {
    return chatItems
      .filter(
        (item) =>
          item.data_type === EMessageDataType.TEXT &&
          typeof item.text === "string" &&
          item.text.trim().length > 0
      )
      .slice(-MAX_ITEMS);
  }, [chatItems]);

  React.useEffect(() => {
    const node = containerRef.current;
    if (!node) {
      return;
    }
    node.scrollTop = node.scrollHeight;
  }, [displayItems]);

  if (!displayItems.length) {
    return null;
  }

  return (
    <div
      className={cn(
        "pointer-events-none absolute bottom-2 left-2 z-20 w-[min(280px,calc(100%-16px))]",
        className
      )}
    >
      <div
        ref={containerRef}
        className="max-h-28 space-y-2 overflow-y-auto rounded-lg bg-black/35 p-2 text-white backdrop-blur-sm"
      >
        {displayItems.map((item, index) => (
          <div
            key={`${item.time}-${item.type}`}
            className={cn(
              "mb-2 max-w-[92%] rounded-md bg-black/45 px-2 py-1.5 leading-snug last:mb-0",
              index < displayItems.length - 1 ? "opacity-70" : "opacity-100"
            )}
          >
            <span
              className={cn("mb-0.5 block text-[9px] font-semibold uppercase tracking-wide", {
                "text-emerald-300": item.type === EMessageType.USER,
                "text-yellow-300": item.type === EMessageType.AGENT,
              })}
            >
              {item.type === EMessageType.USER ? "User" : "Agent"}
            </span>
            <p className="break-words text-xs">{item.text}</p>
          </div>
        ))}
      </div>
    </div>
  );
}
