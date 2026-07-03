"use client";

import dynamic from "next/dynamic";
import type React from "react";
import { forwardRef } from "react";
import type {
  ExpressionConfig,
  Live2DHandle,
  MotionConfig,
  MouthConfig,
} from "./Live2DCharacter";

interface ClientOnlyLive2DProps {
  modelPath: string;
  audioTrack?: any;
  className?: string;
  mouthConfig?: MouthConfig;
  expressions?: ExpressionConfig[];
  motions?: MotionConfig[];
  onModelLoaded?: () => void;
  onModelError?: (error: Error) => void;
}

// Dynamically import the actual Live2D component to avoid SSR issues
const Live2DCharacter = dynamic(() => import("./Live2DCharacter"), {
  ssr: false,
  loading: () => (
    <div className="flex h-full items-center justify-center">
      <div className="text-center">
        <div className="mx-auto mb-4 h-12 w-12 animate-spin rounded-full border-primary border-b-2"></div>
        <p className="text-muted-foreground">Loading Live2D Model...</p>
      </div>
    </div>
  ),
}) as React.ForwardRefExoticComponent<
  React.PropsWithoutRef<ClientOnlyLive2DProps> &
    React.RefAttributes<Live2DHandle>
>;

const ClientOnlyLive2D = forwardRef<Live2DHandle, ClientOnlyLive2DProps>(
  (props, ref) => {
    return <Live2DCharacter {...props} ref={ref} />;
  }
);

ClientOnlyLive2D.displayName = "ClientOnlyLive2D";

export default ClientOnlyLive2D;
export type { Live2DHandle, ExpressionConfig, MotionConfig, MouthConfig };
