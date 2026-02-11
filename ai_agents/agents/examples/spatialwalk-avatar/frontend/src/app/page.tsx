"use client";

import dynamic from "next/dynamic";
import React from "react";
import Avatar from "@/components/Agent/AvatarSpatialwalk";
import AuthInitializer from "@/components/authInitializer";
import EffectOverlay from "@/components/Festival/EffectOverlay";
import FortuneModal from "@/components/Festival/FortuneModal";

const DynamicRTCCard = dynamic(() => import("@/components/Dynamic/RTCCard"), {
  ssr: false,
});

export default function Home() {
  return (
    <AuthInitializer>
      <div className="relative h-screen w-screen overflow-hidden bg-[#181a1d]">
        <EffectOverlay />
        <FortuneModal />
        {/* Keep RTC session lifecycle running, but hide its panel UI. */}
        <DynamicRTCCard className="hidden" />
        <Avatar />
      </div>
    </AuthInitializer>
  );
}
