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
};

export default nextConfig;
