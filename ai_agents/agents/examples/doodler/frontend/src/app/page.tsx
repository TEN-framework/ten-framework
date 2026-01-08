"use client";

import React from "react";
import AuthInitializer from "@/components/authInitializer";
import ImmersiveShell from "@/components/Doodler/ImmersiveShell";

export default function Home() {
  return (
    <AuthInitializer>
      <ImmersiveShell />
    </AuthInitializer>
  );
}
