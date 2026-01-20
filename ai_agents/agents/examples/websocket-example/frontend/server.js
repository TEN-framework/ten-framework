const express = require("express");
const next = require("next");
const { createProxyMiddleware } = require("http-proxy-middleware");
const http = require("node:http");
const { parse } = require("node:url");

const dev = process.env.NODE_ENV !== "production";
const hostname = "localhost";
const port = parseInt(process.env.PORT || "3000", 10);

const app = next({ dev, hostname, port });
const handle = app.getRequestHandler();

// Cache proxy instances per port to prevent memory leaks
const proxyCache = new Map();

/**
 * Get or create a WebSocket proxy for a specific port
 * Reuses existing proxy instances to prevent memory leaks
 */
function getOrCreateProxy(wsPort, devMode) {
  if (!proxyCache.has(wsPort)) {
    proxyCache.set(
      wsPort,
      createProxyMiddleware({
        target: `ws://localhost:${wsPort}`,
        changeOrigin: true,
        ws: true,
        logLevel: devMode ? "debug" : "warn",
        onError: (err, _req, _res) => {
          console.error(`WebSocket proxy error (port ${wsPort}):`, err.message);
        },
        onProxyReqWs: (_proxyReq, req, _socket, _options, _head) => {
          console.log(
            `WebSocket proxy request: ${req.url} -> ws://localhost:${wsPort}`
          );
        },
      })
    );
  }
  return proxyCache.get(wsPort);
}

app.prepare().then(() => {
  const server = express();
  const httpServer = http.createServer(server);

  const agentServerUrl =
    process.env.AGENT_SERVER_URL || "http://localhost:8080";

  // API proxy configuration for agent control endpoints
  const apiProxy = createProxyMiddleware({
    target: agentServerUrl,
    changeOrigin: true,
    pathRewrite: {
      "^/api/agents": "", // Remove /api/agents prefix
    },
    logLevel: dev ? "debug" : "warn",
  });

  // WebSocket proxy with dynamic port support
  // Matches /ws or /ws/{port}
  server.use("/ws", (req, res, next) => {
    // Prevent ReDoS - validate URL length
    if (req.url.length > 50) {
      return res.status(400).json({ error: "Invalid request" });
    }

    // Extract port from path if provided (e.g., /8765)
    // Note: Express middleware strips '/ws' prefix, so req.url is '/8765' not '/ws/8765'
    const portMatch = req.url.match(/^\/(\d+)/);
    const wsPort = portMatch ? portMatch[1] : "8765";

    // Validate port range (8000-9000)
    const portNum = parseInt(wsPort, 10);
    if (portNum < 8000 || portNum > 9000) {
      console.error(
        `Invalid WebSocket port: ${wsPort}. Must be between 8000-9000.`
      );
      return res.status(400).json({ error: "Invalid port number" });
    }

    // Reuse cached proxy instance
    return getOrCreateProxy(wsPort, dev)(req, res, next);
  });

  // Proxy /api/agents/* to the agent server
  server.use("/api/agents", apiProxy);

  // Handle all other requests with Next.js
  server.all("*", (req, res) => {
    return handle(req, res);
  });

  // Listen for upgrade events (WebSocket handshake)
  httpServer.on("upgrade", (req, socket, head) => {
    const { pathname } = parse(req.url || "/", true);

    // Allow Next.js HMR WebSocket connection
    if (pathname === "/_next/webpack-hmr") {
      const upgradeHandler = app.getUpgradeHandler();
      return upgradeHandler(req, socket, head);
    }

    // Handle custom WebSocket proxy to backend
    if (req.url.startsWith("/ws")) {
      // Prevent ReDoS - validate URL length
      if (req.url.length > 50) {
        console.error("Invalid WebSocket upgrade request: URL too long");
        socket.destroy();
        return;
      }

      // Extract port from URL (e.g., /ws/8765)
      // Note: Upgrade handler receives full URL including '/ws' prefix
      const portMatch = req.url.match(/^\/ws\/(\d+)/);
      const wsPort = portMatch ? portMatch[1] : "8765";
      const portNum = parseInt(wsPort, 10);

      if (portNum < 8000 || portNum > 9000) {
        console.error(`Invalid WebSocket port on upgrade: ${wsPort}`);
        socket.destroy();
        return;
      }

      console.log(`WebSocket upgrade: ${req.url} -> ws://localhost:${wsPort}`);

      // Reuse cached proxy instance
      const wsProxy = getOrCreateProxy(wsPort, dev);
      wsProxy.upgrade(req, socket, head);
    } else {
      // Unknown WebSocket path - destroy connection
      socket.destroy();
    }
  });

  httpServer.listen(port, (err) => {
    if (err) throw err;
    console.log(`> Ready on http://${hostname}:${port}`);
    console.log(
      `> WebSocket proxy: /ws/{port} -> ws://localhost:{port} (8000-9000)`
    );
    console.log(`> API proxy: /api/agents -> ${agentServerUrl}`);
  });
});
