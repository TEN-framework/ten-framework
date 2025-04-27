//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
import * as http from "http";

import {
  Addon,
  RegisterAddonAsExtension,
  Extension,
  TenEnv,
  Cmd,
  StatusCode,
  CmdResult,
} from "ten-runtime-nodejs";

class HttpServerExtension extends Extension {
  tenEnv: TenEnv | null = null;
  httpServer: http.Server | null = null;

  constructor(name: string) {
    super(name);
  }

  async onConfigure(tenEnv: TenEnv): Promise<void> {
    console.log("HttpServerExtension onConfigure");
  }

  async onInit(tenEnv: TenEnv): Promise<void> {
    this.tenEnv = tenEnv;
    console.log("HttpServerExtension onInit");
  }

  async handler(
    req: http.IncomingMessage,
    res: http.ServerResponse,
  ): Promise<void> {
    if (
      req.method === "POST" &&
      req.headers["content-type"] === "application/json"
    ) {
      let body = "";

      req.on("data", (chunk: string) => {
        body += chunk;
      });

      req.on("end", () => {
        let jsonData: any;
        try {
          jsonData = JSON.parse(body);

          console.log("Parsed JSON:", jsonData);
        } catch (error) {
          console.error("Error parsing JSON:", error);

          res.writeHead(400, { "Content-Type": "text/plain" });
          res.end("Failed to parse JSON");
          return;
        }

        // if 'ten' not in the JSON data.
        if (!("ten" in jsonData)) {
          console.log("No ten in JSON data");

          res.writeHead(400, { "Content-Type": "text/plain" });
          res.end("No `ten` in JSON data");
          return;
        }

        const ten = jsonData["ten"];
        if ("type" in ten && ten["type"] == "close_app") {
          const closeAppCmd = Cmd.Create("ten:close_app");
          closeAppCmd.setDest("localhost");
          this.tenEnv!.sendCmd(closeAppCmd);

          res.writeHead(200, { "Content-Type": "application/json" });
          res.end(JSON.stringify({ message: "OK" }));
          return;
        } else if ("name" in ten) {
          const name = ten["name"];
          const cmd = Cmd.Create(name);
          cmd.setPropertyFromJson("", body);
          cmd.setPropertyString("method", req.method!);
          cmd.setPropertyString("url", req.url!);

          this.tenEnv!.sendCmd(cmd).then(
            ([cmdResult, error]: [CmdResult | null, Error | null]) => {
              if (error) {
                res.writeHead(500, { "Content-Type": "text/plain" });
                res.end("Error: " + error.message);
              } else {
                if (cmdResult?.getStatusCode() == StatusCode.OK) {
                  const [detail, err] = cmdResult!.getPropertyToJson("detail");
                  res.writeHead(200, { "Content-Type": "application/json" });
                  res.end(detail);
                } else {
                  console.error("Error: " + cmdResult?.getStatusCode());
                  res.writeHead(500, { "Content-Type": "text/plain" });
                  res.end("Internal Server Error");
                }
              }
            },
          );
        } else {
          console.error("Invalid ten");

          res.writeHead(400, { "Content-Type": "text/plain" });
          res.end("Invalid ten");
        }
      });
    }
  }

  async onStart(tenEnv: TenEnv): Promise<void> {
    console.log("HttpServerExtension onStart");

    const hostname = "127.0.0.1";
    let [port, err] = await tenEnv.getPropertyNumber("server_port");
    if (err != null) {
      // Use default port.
      port = 8001;
    }

    // Start a simple http server.
    const server = http.createServer(this.handler.bind(this));

    server.listen(port, () => {
      console.log("Server running at http://" + hostname + ":" + port + "/");
    });

    this.httpServer = server;
  }

  async onStop(_tenEnv: TenEnv): Promise<void> {
    console.log("HttpServerExtension onStop");

    await new Promise<void>((resolve, reject) => {
      this.httpServer!.close((err: any) => {
        if (err) {
          reject(err);
        } else {
          resolve();
        }
      });
    });
  }

  async onDeinit(_tenEnv: TenEnv): Promise<void> {
    this.tenEnv = null;
    console.log("HttpServerExtension onDeinit");
  }
}

@RegisterAddonAsExtension("http_server_extension_nodejs")
class HttpServerExtensionAddon extends Addon {
  async onInit(_tenEnv: TenEnv): Promise<void> {
    console.log("DefaultAddon onInit");
  }

  async onCreateInstance(
    _tenEnv: TenEnv,
    instanceName: string,
  ): Promise<Extension> {
    return new HttpServerExtension(instanceName);
  }

  async onDeinit(_tenEnv: TenEnv): Promise<void> {
    console.log("DefaultAddon onDeinit");
  }
}
