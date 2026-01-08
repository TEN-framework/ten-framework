"use client";

import * as React from "react";
import { cn } from "@/lib/utils";
import { useAppDispatch, useAppSelector } from "@/common";
import { addChatItem, setOptions, setRoomConnected } from "@/store/reducers/global";
import DoodleCanvas from "./Canvas";
import ControlsBar from "./ControlsBar";
import LoadingAnimator from "./LoadingAnimator";
import { EMessageDataType, EMessageType, type IChatItem } from "@/types";

export default function AppShell() {
  const dispatch = useAppDispatch();
  const options = useAppSelector((s) => s.global.options);
  const [channel, setChannel] = React.useState(options.channel || "voice_image_kids");
  const [userId, setUserId] = React.useState<number>(options.userId || Math.floor(100000 + Math.random() * 900000));
  const rtcRef = React.useRef<any>(null);

  React.useEffect(() => {
    let mounted = true;
    import("@/manager/rtc/rtc").then((m) => {
      if (mounted) rtcRef.current = m.rtcManager;
    });
    return () => {
      mounted = false;
    };
  }, []);

  const connect = async () => {
    const { apiStartService } = await import("@/common");
    await apiStartService({
      channel,
      userId,
      graphName: "voice_image_kids",
      language: "en-US",
      voiceType: "female",
    });
    const rtc = rtcRef.current;
    rtc.on("textChanged", (text: IChatItem) => dispatch(addChatItem(text)));
    rtc.on("localTracksChanged", () => {});
    rtc.on("remoteUserChanged", () => {});
    await rtc.createMicrophoneAudioTrack();
    await rtc.join({ channel, userId });
    dispatch(
      setOptions({
        ...options,
        channel,
        userId,
        appId: rtc.appId ?? "",
        token: rtc.token ?? "",
      })
    );
    await rtc.publish();
    dispatch(setRoomConnected(true));
  };

  const disconnect = async () => {
    await rtcRef.current?.destroy();
    dispatch(setRoomConnected(false));
  };

  return (
    <div className={cn("flex min-h-screen flex-col bg-[#121316]")}>
      <header
        className={cn(
          "flex items-center justify-between px-4 py-3",
          "shadow-md bg-[#1b1d22]"
        )}
      >
        <h1
          className={cn(
            "text-2xl font-extrabold tracking-wide",
            "text-[#FF6B6B]"
          )}
          aria-label="Doodler"
        >
          Doodler
        </h1>
        <div className="flex gap-2">
          <input
            type="text"
            value={channel}
            onChange={(e) => setChannel(e.target.value)}
            aria-label="Channel"
          className={cn(
            "rounded-lg border border-[#2b2e35] bg-[#121316] px-3 py-2 text-sm text-white"
          )}
          />
          <input
            type="number"
            value={userId}
            onChange={(e) => setUserId(Number(e.target.value))}
            aria-label="User ID"
          className={cn(
            "rounded-lg border border-[#2b2e35] bg-[#121316] px-3 py-2 text-sm text-white"
          )}
          />
          <button
            type="button"
            onClick={connect}
            className={cn(
              "rounded-lg px-3 py-2 text-sm font-semibold",
              "bg-[#ffd166] text-black"
            )}
            aria-label="Connect"
          >
            Connect
          </button>
          <button
            type="button"
            onClick={disconnect}
            className={cn(
              "rounded-lg px-3 py-2 text-sm font-semibold",
              "bg-[#2b2e35] text-white"
            )}
            aria-label="Disconnect"
          >
            Disconnect
          </button>
          <span
            className={cn(
              "rounded-full px-3 py-1 text-sm",
              "bg-[#2b2e35] text-[#ffd166]"
            )}
          >
            Kid Mode
          </span>
        </div>
      </header>
      <main className="flex flex-1 flex-col gap-3 p-3 md:flex-row">
        <section
          className={cn(
            "flex flex-1 items-center justify-center rounded-xl",
            "p-2 bg-[#181a1d]"
          )}
          aria-label="Drawing canvas"
        >
          <DoodleCanvas />
          <LoadingAnimator />
        </section>
        <aside
          className={cn(
            "w-full md:w-[380px] rounded-xl bg-[#181a1d] p-3",
            "border border-[#262a31]"
          )}
          aria-label="Controls"
        >
          <ControlsBar />
        </aside>
      </main>
    </div>
  );
}
