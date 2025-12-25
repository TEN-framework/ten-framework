/** @type {import('next').NextConfig} */
const nextConfig = {
  output: "standalone",
  reactStrictMode: false, // Disable strict mode to prevent double mounting issues with PIXI
  webpack: (config, { webpack }) => {
    // Provide PIXI as a global variable for pixi-live2d-display
    config.plugins.push(
      new webpack.ProvidePlugin({
        PIXI: "pixi.js",
      })
    );

    return config;
  },
  env: {
    // Make AGENT_SERVER_URL available to middleware
    AGENT_SERVER_URL: process.env.AGENT_SERVER_URL || "http://localhost:8080",
  },
};

export default nextConfig;
