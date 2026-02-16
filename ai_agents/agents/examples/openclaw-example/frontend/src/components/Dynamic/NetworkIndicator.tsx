"use client";

import type { NetworkQuality } from "agora-rtc-sdk-ng";
import * as React from "react";
import { NetworkIconByLevel } from "@/components/Icon";
import { rtcManager } from "@/manager";

export default function NetworkIndicator() {
  const [networkQuality, setNetworkQuality] = React.useState<NetworkQuality>();

  const onNetworkQuality = (quality: NetworkQuality) => {
    setNetworkQuality(quality);
  };

  React.useEffect(() => {
    rtcManager.on("networkQuality", onNetworkQuality);

    return () => {
      rtcManager.off("networkQuality", onNetworkQuality);
    };
  }, [onNetworkQuality]);

  return (
    <NetworkIconByLevel
      level={networkQuality?.uplinkNetworkQuality}
      className="h-4 w-4 md:h-5 md:w-5"
    />
  );
}
