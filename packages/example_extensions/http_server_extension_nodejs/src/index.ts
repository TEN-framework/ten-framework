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
  TenError,
} from "ten-runtime-nodejs";

class HttpServerExtension extends Extension {
  tenEnv: TenEnv | undefined = undefined;
  httpServer: http.Server | undefined = undefined;

  constructor(name: string) {
    super(name);
  }

  async onConfigure(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("HttpServerExtension onConfigure");
  }

  async onInit(tenEnv: TenEnv): Promise<void> {
    this.tenEnv = tenEnv;
    tenEnv.logInfo("HttpServerExtension onInit");
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
          closeAppCmd.setDests([{ appUri: "" }]);
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
            ([cmdResult, error]: [
              CmdResult | undefined,
              TenError | undefined,
            ]) => {
              if (error) {
                res.writeHead(500, { "Content-Type": "text/plain" });
                res.end("Error: " + error.errorMessage);
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
    tenEnv.logInfo("HttpServerExtension onStart");

    const hostname = "127.0.0.1";
    let [port, err] = await tenEnv.getPropertyNumber("server_port");
    if (err != undefined) {
      // Use default port.
      port = 8001;
    }

    // Start a simple http server.
    const server = http.createServer(this.handler.bind(this));

    server.listen(port, () => {
      tenEnv.logInfo("Server running at http://" + hostname + ":" + port + "/");
    });

    this.httpServer = server;
  }

  async onStop(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("HttpServerExtension onStop");

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

  async onDeinit(tenEnv: TenEnv): Promise<void> {
    this.tenEnv = undefined;
    tenEnv.logInfo("HttpServerExtension onDeinit");
  }
}

@RegisterAddonAsExtension("http_server_extension_nodejs")
class HttpServerExtensionAddon extends Addon {
  async onCreateInstance(
    _tenEnv: TenEnv,
    instanceName: string,
  ): Promise<Extension> {
    return new HttpServerExtension(instanceName);
  }
}
