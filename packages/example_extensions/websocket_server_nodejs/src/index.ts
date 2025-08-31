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
  tenEnv: TenEnv | undefined = undefined;
  httpServer: http.Server | undefined = undefined;

  constructor(name: string) {
    super(name);
  }

  async onConfigure(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("WebsocketServerExtension onConfigure");
  }

  async onInit(tenEnv: TenEnv): Promise<void> {
    this.tenEnv = tenEnv;
    tenEnv.logInfo("WebsocketServerExtension onInit");
  }

  async onStart(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("WebsocketServerExtension onStart");

    const hostname = "127.0.0.1";
    let [port, err] = await tenEnv.getPropertyNumber("server_port");
    if (err != undefined) {
      // Use default port.
      port = 8001;
    }

    const app = express();

    const server = app.listen(port, () => {
      tenEnv.logInfo("Server running at http://" + hostname + ":" + port + "/");
    });

    this.httpServer = server;

    const wss = new WebSocketServer({ server });

    wss.on("connection", (ws) => {
      tenEnv.logInfo("Client connected");

      ws.on("message", (message) => {
        tenEnv.logInfo("Received: " + message);
        ws.send("Echo: " + message);
      });

      ws.on("close", () => {
        tenEnv.logInfo("Client disconnected");
      });
    });
  }

  async onStop(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("WebsocketServerExtension onStop");

    if (this.httpServer) {
      this.httpServer.close();
    }
  }

  async onDeinit(tenEnv: TenEnv): Promise<void> {
    this.tenEnv = undefined;
    tenEnv.logInfo("WebsocketServerExtension onDeinit");
  }
}

@RegisterAddonAsExtension("websocket_server_nodejs")
class WebsocketServerExtensionAddon extends Addon {
  async onCreateInstance(
    _tenEnv: TenEnv,
    instanceName: string,
  ): Promise<Extension> {
    return new WebsocketServerExtension(instanceName);
  }
}
