/** @type {import('next').NextConfig} */

const svgLoader = {
  loader: "@svgr/webpack",
  options: {
    icon: true,
  },
};

const nextConfig = {
  // basePath: '/ai-agent',
  // output: 'export',
  output: "standalone",
  reactStrictMode: false,
  turbopack: {
    rules: {
      "*.svg": {
        loaders: [svgLoader],
        as: "*.js",
      },
    },
  },
  webpack(config) {
    // Grab the existing rule that handles SVG imports so we can customize only SVG behavior.
    const fileLoaderRule = config.module.rules.find((rule) =>
      rule.test?.test?.(".svg"),
    );

    if (!fileLoaderRule) {
      return config;
    }

    const resourceQueryNot = Array.isArray(fileLoaderRule.resourceQuery?.not)
      ? fileLoaderRule.resourceQuery.not
      : [];

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
        resourceQuery: { not: [...resourceQueryNot, /url/] }, // exclude if *.svg?url
        use: [svgLoader],
      },
    );

    // Modify the file loader rule to ignore *.svg, since we have it handled now.
    fileLoaderRule.exclude = /\.svg$/i;

    return config;
  },
};

export default nextConfig;
