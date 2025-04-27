//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
import { WebSocketServer } from "ws";
import express from "express";
import * as http from "http";

import {
  Addon,
  RegisterAddonAsExtension,
  Extension,
  TenEnv,
} from "ten-runtime-nodejs";

class WebsocketServerExtension extends Extension {
  tenEnv: TenEnv | null = null;
  httpServer: http.Server | null = null;

  constructor(name: string) {
    super(name);
  }

  async onConfigure(tenEnv: TenEnv): Promise<void> {
    console.log("WebsocketServerExtension onConfigure");
  }

  async onInit(tenEnv: TenEnv): Promise<void> {
    this.tenEnv = tenEnv;
    console.log("WebsocketServerExtension onInit");
  }

  async onStart(tenEnv: TenEnv): Promise<void> {
    console.log("WebsocketServerExtension onStart");

    const hostname = "127.0.0.1";
    let [port, err] = await tenEnv.getPropertyNumber("server_port");
    if (err != null) {
      // Use default port.
      port = 8001;
    }

    const app = express();

    const server = app.listen(port, () => {
      console.log("Server running at http://" + hostname + ":" + port + "/");
    });

    this.httpServer = server;

    const wss = new WebSocketServer({ server });

    wss.on("connection", (ws) => {
      console.log("Client connected");

      ws.on("message", (message) => {
        console.log("Received: " + message);
        ws.send("Echo: " + message);
      });

      ws.on("close", () => {
        console.log("Client disconnected");
      });
    });
  }

  async onStop(_tenEnv: TenEnv): Promise<void> {
    console.log("WebsocketServerExtension onStop");

    if (this.httpServer) {
      this.httpServer.close();
    }
  }

  async onDeinit(_tenEnv: TenEnv): Promise<void> {
    this.tenEnv = null;
    console.log("WebsocketServerExtension onDeinit");
  }
}

@RegisterAddonAsExtension("websocket_server_nodejs")
class WebsocketServerExtensionAddon extends Addon {
  async onInit(_tenEnv: TenEnv): Promise<void> {
    console.log("WS server addon onInit");
  }

  async onCreateInstance(
    _tenEnv: TenEnv,
    instanceName: string,
  ): Promise<Extension> {
    return new WebsocketServerExtension(instanceName);
  }

  async onDeinit(_tenEnv: TenEnv): Promise<void> {
    console.log("WS server addon onDeinit");
  }
}
