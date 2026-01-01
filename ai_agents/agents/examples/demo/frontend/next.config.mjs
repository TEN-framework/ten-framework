/** @type {import('next').NextConfig} */

import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

// Derive __dirname in ESM
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const nextConfig = {
  // basePath: '/ai-agent',
  // output: 'export',
  output: "standalone",
  reactStrictMode: false,
  webpack(config) {
    // Grab the existing rule that handles SVG imports
    const fileLoaderRule = config.module.rules.find((rule) =>
      rule.test?.test?.(".svg")
    );

    config.module.rules.push(
      // Reapply the existing rule, but only for svg imports ending in ?url
      {
        ...fileLoaderRule,
        test: /\.svg$/i,
        resourceQuery: /url/, // *.svg?url
      },
      // Convert all other *.svg imports to React components
      {
        test: /\.svg$/i,
        issuer: fileLoaderRule.issuer,
        resourceQuery: { not: [...fileLoaderRule.resourceQuery.not, /url/] }, // exclude if *.svg?url
        use: ["@svgr/webpack"],
      }
    );

    // Modify the file loader rule to ignore *.svg, since we have it handled now.
    fileLoaderRule.exclude = /\.svg$/i;

    // Ensure TS path alias `@/*` resolves to `src/*` in webpack too
    config.resolve = config.resolve || {};
    config.resolve.alias = {
      ...(config.resolve.alias || {}),
      "@": path.resolve(__dirname, "src"),
    };

    // Debug logs to verify alias + file presence during CI builds
    try {
      const aliasPath = config.resolve.alias["@"];
      const utilPath = path.join(aliasPath, "lib", "utils.ts");
      // eslint-disable-next-line no-console
      console.log(
        "[next.config] alias @ →",
        aliasPath,
        "| utils.ts exists =",
        fs.existsSync(utilPath)
      );
    } catch (e) {
      // eslint-disable-next-line no-console
      console.log("[next.config] alias debug error:", e?.message);
    }

    return config;
  },
};

export default nextConfig;
