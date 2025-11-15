const express = require('express');
const next = require('next');
const { createProxyMiddleware } = require('http-proxy-middleware');
const http = require('http');
const { parse } = require('url');

const dev = process.env.NODE_ENV !== 'production';
const hostname = 'localhost';
const port = parseInt(process.env.PORT || '3000', 10);

const app = next({ dev, hostname, port });
const handle = app.getRequestHandler();

app.prepare().then(() => {
  const server = express();
  const httpServer = http.createServer(server);

  const agentServerUrl = process.env.AGENT_SERVER_URL || 'http://localhost:8080';

  // API proxy configuration for agent control endpoints
  const apiProxy = createProxyMiddleware({
    target: agentServerUrl,
    changeOrigin: true,
    pathRewrite: {
      '^/api/agents': '', // Remove /api/agents prefix
    },
    logLevel: dev ? 'debug' : 'warn',
  });

  // WebSocket proxy with dynamic port support
  // Matches /ws or /ws/{port}
  server.use('/ws', (req, res, next) => {
    // Extract port from path if provided (e.g., /ws/8765)
    const portMatch = req.url.match(/^\/(\d+)/);
    const wsPort = portMatch ? portMatch[1] : '8765';

    // Validate port range (8000-9000)
    const portNum = parseInt(wsPort, 10);
    if (portNum < 8000 || portNum > 9000) {
      console.error(`Invalid WebSocket port: ${wsPort}. Must be between 8000-9000.`);
      return res.status(400).json({ error: 'Invalid port number' });
    }

    const wsTarget = `ws://localhost:${wsPort}`;

    const wsProxy = createProxyMiddleware({
      target: wsTarget,
      changeOrigin: true,
      ws: true,
      logLevel: dev ? 'debug' : 'warn',
      onError: (err, req, res) => {
        console.error('WebSocket proxy error:', err.message);
      },
      onProxyReqWs: (proxyReq, req, socket, options, head) => {
        console.log(`WebSocket proxy request: ${req.url} -> ${wsTarget}`);
      },
    });

    return wsProxy(req, res, next);
  });

  // Proxy /api/agents/* to the agent server
  server.use('/api/agents', apiProxy);

  // Handle all other requests with Next.js
  server.all('*', (req, res) => {
    return handle(req, res);
  });

  // Listen for upgrade events (WebSocket handshake)
  httpServer.on('upgrade', (req, socket, head) => {
    const { pathname } = parse(req.url || '/', true);

    // Allow Next.js HMR WebSocket connection
    if (pathname === '/_next/webpack-hmr') {
      const upgradeHandler = app.getUpgradeHandler();
      return upgradeHandler(req, socket, head);
    }

    // Handle custom WebSocket proxy to backend
    if (req.url.startsWith('/ws')) {
      // Extract port from URL
      const portMatch = req.url.match(/^\/ws\/(\d+)/);
      const wsPort = portMatch ? portMatch[1] : '8765';
      const portNum = parseInt(wsPort, 10);

      if (portNum < 8000 || portNum > 9000) {
        console.error(`Invalid WebSocket port on upgrade: ${wsPort}`);
        socket.destroy();
        return;
      }

      const wsTarget = `ws://localhost:${wsPort}`;
      console.log(`WebSocket upgrade: ${req.url} -> ${wsTarget}`);

      const wsProxy = createProxyMiddleware({
        target: wsTarget,
        changeOrigin: true,
        ws: true,
      });

      wsProxy.upgrade(req, socket, head);
    } else {
      // Unknown WebSocket path - destroy connection
      socket.destroy();
    }
  });

  httpServer.listen(port, (err) => {
    if (err) throw err;
    console.log(`> Ready on http://${hostname}:${port}`);
    console.log(`> WebSocket proxy: /ws/{port} -> ws://localhost:{port} (8000-9000)`);
    console.log(`> API proxy: /api/agents -> ${agentServerUrl}`);
  });
});
