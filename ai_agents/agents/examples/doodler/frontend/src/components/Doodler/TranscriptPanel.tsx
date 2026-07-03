"use client";

import * as React from "react";
import { useAppSelector, useAutoScroll } from "@/common";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { cn } from "@/lib/utils";
import { EMessageDataType, EMessageType, type IChatItem } from "@/types";

function getTranscriptItems(items: IChatItem[]) {
  return items.filter(
    (i) =>
      i.data_type === EMessageDataType.TEXT &&
      typeof i.text === "string" &&
      i.text.trim().length > 0
  );
}

export default function TranscriptPanel(props: {
  className?: string;
  disabled?: boolean;
  onSend?: (text: string) => Promise<void> | void;
  placeholder?: string;
  style?: React.CSSProperties;
}) {
  const { className, disabled = false, onSend, placeholder, style } = props;
  const items = useAppSelector((s) => s.global.chatItems);
  const transcript = React.useMemo(() => getTranscriptItems(items), [items]);
  const containerRef = React.useRef<HTMLDivElement | null>(null);
  const [value, setValue] = React.useState("");

  useAutoScroll(containerRef);

  const canSend = !disabled && Boolean(onSend) && value.trim().length > 0;

  const onSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!canSend || !onSend) return;
    await onSend(value);
    setValue("");
  };

  return (
    <section
      aria-label="Transcript"
      className={cn(
        "flex min-h-0 flex-col overflow-hidden",
        "crayon-border shadow-[0_18px_50px_rgba(34,18,10,0.12)] backdrop-blur-md",
        className
      )}
      style={style}
    >
      <div className="flex items-center justify-between gap-2 border-black/20 border-b border-dashed px-4 py-3">
        <div className="flex items-center gap-2">
          <span className="h-2.5 w-2.5 rounded-full bg-[#F97316]" aria-hidden />
          <h2 className="font-semibold text-sm">Transcript</h2>
        </div>
        <span className="text-muted-foreground text-xs">
          {transcript.length ? `${transcript.length}` : "—"}
        </span>
      </div>

      <div ref={containerRef} className="min-h-0 flex-1 overflow-y-auto p-3">
        {transcript.length ? (
          <div className="space-y-3 py-1">
            {transcript.map((item, idx) => {
              const isUser = item.type === EMessageType.USER;
              return (
                <div
                  // biome-ignore lint/suspicious/noArrayIndexKey: transcript list is append-only-ish and sorted by time
                  key={`${item.time}-${idx}`}
                  className={cn("flex", isUser ? "justify-end" : "justify-start")}
                >
                  <div
                    className={cn(
                      "max-w-[92%] px-3 py-2 text-sm leading-relaxed crayon-bubble",
                      isUser
                        ? "border-[#F97316]/20 bg-[#F97316]/12"
                        : "border-black/10 bg-white/60",
                      item.isFinal === false ? "opacity-70" : ""
                    )}
                  >
                    <div className="mb-1 flex items-center justify-between gap-2 text-[10px] text-muted-foreground uppercase tracking-wide">
                      <span>{isUser ? "You" : "Agent"}</span>
                      {item.isFinal === false ? <span>…</span> : null}
                    </div>
                    <div className="whitespace-pre-wrap break-words">
                      {item.text}
                    </div>
                  </div>
                </div>
              );
            })}
          </div>
        ) : (
          <p className="px-2 py-3 text-muted-foreground text-sm">
            Your transcript will show up here.
          </p>
        )}
      </div>

      {onSend ? (
        <form
          onSubmit={onSubmit}
          className="border-black/20 border-t border-dashed bg-white/55 p-3"
        >
          <div className="flex items-center gap-2">
            <Input
              value={value}
              onChange={(e) => setValue(e.target.value)}
              placeholder={placeholder ?? "Type a prompt…"}
              className="h-11 bg-white/80 crayon-control"
              disabled={disabled}
            />
            <Button
              type="submit"
              className="h-11 px-5 shadow-none crayon-control"
              disabled={!canSend}
            >
              Send
            </Button>
          </div>
        </form>
      ) : null}
    </section>
  );
}
