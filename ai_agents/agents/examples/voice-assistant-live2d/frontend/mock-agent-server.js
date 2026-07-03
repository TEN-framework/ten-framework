const http = require("node:http");

const server = http.createServer((req, res) => {
  // Set CORS headers
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.setHeader(
    "Access-Control-Allow-Methods",
    "GET, POST, PUT, DELETE, OPTIONS"
  );
  res.setHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");

  if (req.method === "OPTIONS") {
    res.writeHead(200);
    res.end();
    return;
  }

  if (req.url === "/token/generate" && req.method === "POST") {
    let body = "";
    req.on("data", (chunk) => {
      body += chunk.toString();
    });

    req.on("end", () => {
      try {
        const data = JSON.parse(body);
        console.log("Received token request:", data);

        // Mock response
        const response = {
          app_id: "mock-agora-app-id",
          channel_name: data.channel_name,
          token: "mock-token-12345",
          uid: data.uid,
        };

        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(JSON.stringify(response));
      } catch (_error) {
        res.writeHead(400, { "Content-Type": "application/json" });
        res.end(JSON.stringify({ error: "Invalid JSON" }));
      }
    });
  } else {
    res.writeHead(404, { "Content-Type": "application/json" });
    res.end(JSON.stringify({ error: "Not found" }));
  }
});

const PORT = 8080;
server.listen(PORT, () => {
  console.log(`Mock agent server running on port ${PORT}`);
  console.log(
    `Test with: curl -X POST http://localhost:${PORT}/token/generate -H 'Content-Type: application/json' -d '{"request_id":"test","uid":12345,"channel_name":"test-channel"}'`
  );
});
