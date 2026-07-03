"use client";

import * as React from "react";
import { useAppSelector } from "@/common";
import { cn } from "@/lib/utils";
import { EMessageDataType, EMessageType } from "@/types";

export default function LoadingAnimator() {
  const items = useAppSelector((s) => s.global.chatItems);
  const lastImageTime = React.useMemo(() => {
    const img = [...items].reverse().find((i) => i.data_type === EMessageDataType.IMAGE);
    return img?.time ?? 0;
  }, [items]);
  const lastUserTime = React.useMemo(() => {
    const usr = [...items]
      .reverse()
      .find(
        (i) => i.type === EMessageType.USER && i.data_type === EMessageDataType.TEXT
      );
    return usr?.time ?? 0;
  }, [items]);
  const active = lastUserTime > lastImageTime;
  return (
    <div
      aria-live="polite"
      className={cn(
        "pointer-events-none absolute inset-0 flex items-center justify-center",
        active ? "opacity-100" : "opacity-0"
      )}
    >
      <div className="relative h-32 w-32">
        <div className="absolute inset-0 animate-ping rounded-full bg-[#ffd166]/30" />
        <div className="absolute inset-3 animate-spin rounded-full border-4 border-[#FF6B6B] border-t-transparent" />
        <div className="absolute inset-0 flex items-center justify-center text-[#ffd166]">
          Doodling...
        </div>
      </div>
    </div>
  );
}
