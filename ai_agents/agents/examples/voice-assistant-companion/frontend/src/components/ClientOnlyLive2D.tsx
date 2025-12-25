"use client";

import dynamic from "next/dynamic";

// IRemoteAudioTrack will be imported dynamically

interface ClientOnlyLive2DProps {
  modelPath: string;
  audioTrack?: any;
  className?: string;
  onModelLoaded?: () => void;
  onModelError?: (error: Error) => void;
}

// Dynamically import the actual Live2D component with no SSR
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
});

export default function ClientOnlyLive2D(props: ClientOnlyLive2DProps) {
  return <Live2DCharacter {...props} />;
}
