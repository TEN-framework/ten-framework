//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import path from "node:path";
import react from "@vitejs/plugin-react-swc";
import { defineConfig } from "vite";
import { nodePolyfills } from "vite-plugin-node-polyfills";

export default defineConfig({
  define: {
    global: "globalThis",
  },
  optimizeDeps: {
    include: ["buffer", "crypto", "process"],
  },
  plugins: [
    react(),
    nodePolyfills({
      protocolImports: true,
      globals: {
        process: true,
        Buffer: true,
      },
    }),
  ],
  resolve: {
    alias: {
      "@": path.resolve(__dirname, "./src"),
      crypto: "crypto-browserify",
      buffer: "buffer",
      process: "process/browser",
    },
  },
  server: {
    host: true,
    proxy: {
      "/api": {
        target:
          process.env.VITE_TMAN_GD_BACKEND_HTTP_ENDPOINT ||
          "http://localhost:49483",
        changeOrigin: true,
        secure: false,
      },
    },
  },
});
