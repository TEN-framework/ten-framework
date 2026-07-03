"use client";

import * as React from "react";
import { useAppSelector } from "@/common";
import { Button } from "@/components/ui/button";
import { cn } from "@/lib/utils";
import { rtmManager } from "@/manager/rtm";
import { EMessageDataType, type IChatItem } from "@/types";

const STYLES = ["cartoon", "watercolor", "crayon", "pixel art"];

export default function DoodlePanel(props: { className?: string }) {
  const { className } = props;
  const chatItems = useAppSelector((state) => state.global.chatItems);
  const options = useAppSelector((state) => state.global.options);
  const agentConnected = useAppSelector((state) => state.global.agentConnected);
  const rtmConnected = useAppSelector((state) => state.global.rtmConnected);

  const images = React.useMemo(
    () => chatItems.filter((i) => i.data_type === EMessageDataType.IMAGE),
    [chatItems]
  );
  const [selectedIndex, setSelectedIndex] = React.useState(
    images.length ? images.length - 1 : -1
  );
  React.useEffect(() => {
    if (images.length) {
      setSelectedIndex(images.length - 1);
    }
  }, [images.length]);

  const disableControls =
    !options.channel ||
    !options.userId ||
    !options.appId ||
    !options.token ||
    !rtmConnected ||
    !agentConnected;

  const current: IChatItem | undefined =
    selectedIndex >= 0 ? images[selectedIndex] : undefined;

  const [refine, setRefine] = React.useState("");

  const handleRefineSubmit = (e: React.FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    if (!refine || disableControls) return;
    rtmManager.sendText(refine);
    setRefine("");
  };

  const handleStyleClick = (style: string) => {
    if (disableControls) return;
    rtmManager.sendText(`Make the picture ${style} style`);
  };

  return (
    <div
      className={cn(
        "flex h-full min-h-0 flex-col gap-2 p-4",
        "rounded-b-lg bg-[#181a1d] md:rounded-lg",
        className
      )}
    >
      <div className="flex items-center justify-between">
        <h3 className="font-medium text-sm">Doodle Panel</h3>
        <div className="flex gap-2">
          {STYLES.map((s) => (
            <Button
              key={s}
              size="sm"
              variant="outline"
              className={cn("bg-transparent", {
                ["cursor-not-allowed opacity-50"]: disableControls,
              })}
              onClick={() => handleStyleClick(s)}
              disabled={disableControls}
            >
              {s}
            </Button>
          ))}
        </div>
      </div>

      <div className="flex-1 overflow-auto rounded-lg border p-2">
        {current ? (
          <img
            src={current.text}
            alt="current doodle"
            className="mx-auto max-h-[320px] w-auto"
          />
        ) : (
          <p className="text-muted-foreground text-sm">
            No doodles yet. Describe your idea to start!
          </p>
        )}
      </div>

      <div className="flex gap-2 overflow-x-auto">
        {images.map((img, idx) => (
          <button
            key={`${img.time}-${idx}`}
            type="button"
            className={cn(
              "h-20 w-20 flex-shrink-0 overflow-hidden rounded border",
              idx === selectedIndex ? "ring-2 ring-ring" : ""
            )}
            onClick={() => setSelectedIndex(idx)}
          >
            <img src={img.text} alt={`v${idx + 1}`} className="h-full w-full" />
          </button>
        ))}
      </div>

      <form onSubmit={handleRefineSubmit} className="flex items-center gap-2">
        <input
          type="text"
          placeholder="Try a change (e.g., add a rainbow)"
          value={refine}
          onChange={(e) => setRefine(e.target.value)}
          className={cn(
            "grow rounded-md border bg-background p-1.5 focus:outline-hidden focus:ring-1 focus:ring-ring",
            {
              ["cursor-not-allowed"]: disableControls,
            }
          )}
          disabled={disableControls}
        />
        <Button
          type="submit"
          size="sm"
          variant="outline"
          className={cn("bg-transparent", {
            ["cursor-not-allowed opacity-50"]:
              disableControls || refine.length === 0,
          })}
          disabled={disableControls || refine.length === 0}
        >
          Apply change
        </Button>
      </form>
    </div>
  );
}
