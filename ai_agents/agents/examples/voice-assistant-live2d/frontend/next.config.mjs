/** @type {import('next').NextConfig} */
const nextConfig = {
  output: 'standalone',
  reactStrictMode: false, // Disable strict mode to prevent double mounting issues with PIXI
  serverExternalPackages: ['pixi.js', 'pixi-live2d-display'],
  webpack: (config) => {
    config.externals.push({
      'pixi.js': 'PIXI',
      'pixi-live2d-display': 'PIXI.live2d',
    });
    return config;
  },
};

export default nextConfig;
