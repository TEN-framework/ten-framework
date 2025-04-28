//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { StrictMode } from "react";
import { createRoot } from "react-dom/client";

import App from "@/App.tsx";
import "@/i18n";
import { Toaster } from "@/components/ui/Sonner";

import "@/index.css";
import "@/fonts.css";

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <App />
    <Toaster richColors duration={6000} />
  </StrictMode>
);
