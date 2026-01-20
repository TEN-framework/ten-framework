"use client";

import * as React from "react";
import { useAppSelector } from "@/common";
import { cn } from "@/lib/utils";

const STYLES = ["cartoon", "crayon", "watercolor"];

export default function ControlsBar() {
  const options = useAppSelector((s) => s.global.options);
  const agentConnected = useAppSelector((s) => s.global.agentConnected);
  const rtmConnected = useAppSelector((s) => s.global.rtmConnected);
  const disable = !options.channel || !options.userId || !rtmConnected || !agentConnected;
  const [val, setVal] = React.useState("");
  const rtmRef = React.useRef<any>(null);

  React.useEffect(() => {
    let mounted = true;
    // Dynamically import to avoid SSR evaluating window-dependent code
    import("@/manager/rtm")
      .then((m) => {
        if (mounted) rtmRef.current = m.rtmManager;
      })
      .catch(() => {});
    return () => {
      mounted = false;
    };
  }, []);

  const submit = (e: React.FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    if (!val || disable) return;
    rtmRef.current?.sendText(val);
    setVal("");
  };

  const styleClick = (s: string) => {
    if (disable) return;
    rtmRef.current?.sendText(`Make it ${s} style`);
  };

  return (
    <div className="flex flex-col gap-3">
      <div className="flex gap-2">
        {STYLES.map((s) => (
          <button
            key={s}
            type="button"
            className={cn(
              "rounded-xl px-4 py-3 text-sm font-semibold",
              "bg-[#2b2e35] text-white",
              { ["opacity-50 cursor-not-allowed"]: disable }
            )}
            onClick={() => styleClick(s)}
            disabled={disable}
            aria-label={`Style ${s}`}
          >
            {s}
          </button>
        ))}
      </div>
      <form onSubmit={submit} className="flex gap-2">
        <input
          type="text"
          value={val}
          onChange={(e) => setVal(e.target.value)}
          placeholder="Tell Doodler what to draw"
          className={cn(
            "grow rounded-xl border border-[#2b2e35] bg-[#15161a] p-3 text-white",
            { ["cursor-not-allowed"]: disable }
          )}
          aria-label="Prompt input"
          disabled={disable}
        />
        <button
          type="submit"
          className={cn(
            "rounded-xl px-5 py-3 text-base font-bold",
            "bg-[#FF6B6B] text-black focus:outline-hidden focus:ring-2 focus:ring-[#ffd166]",
            { ["opacity-50 cursor-not-allowed"]: disable || val.length === 0 }
          )}
          disabled={disable || val.length === 0}
          aria-label="Doodle It!"
        >
          Doodle It!
        </button>
      </form>
      <div className="text-xs text-[#ffd166]">Sounds can be toggled in Settings.</div>
    </div>
  );
}
