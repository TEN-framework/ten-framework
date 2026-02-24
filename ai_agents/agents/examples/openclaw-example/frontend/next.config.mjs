/** @type {import('next').NextConfig} */

import path from "node:path";
import { fileURLToPath } from "node:url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const nextConfig = {
  // basePath: '/ai-agent',
  // output: 'export',
  output: "standalone",
  reactStrictMode: false,
  // this includes files from the monorepo base two directories up
  outputFileTracingRoot: path.join(__dirname, "./"),
};

export default nextConfig;
