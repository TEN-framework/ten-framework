"use client";

import * as React from "react";
import { useAppSelector } from "@/common";
import { EMessageDataType, type IChatItem } from "@/types";
import { cn } from "@/lib/utils";

export default function DoodleCanvas() {
  const chatItems = useAppSelector((s) => s.global.chatItems);
  const images = React.useMemo(
    () => chatItems.filter((i) => i.data_type === EMessageDataType.IMAGE),
    [chatItems]
  );
  const current: IChatItem | undefined = images.length
    ? images[images.length - 1]
    : undefined;
  return (
    <div
      className={cn(
        "relative flex w-full h-[60vh] items-center justify-center",
        "rounded-2xl border-2 border-dashed border-[#2b2e35]",
        "bg-[#15161a]"
      )}
    >
      {current ? (
        <img
          src={current.text}
          alt="doodle"
          className="max-h-full max-w-full"
        />
      ) : (
        <p className="text-center text-[#ffd166]">Describe your idea to start!</p>
      )}
    </div>
  );
}
