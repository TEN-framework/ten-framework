//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import ten_addon from "../ten_addon.js";
import { Msg } from "./msg.js";

export class Cmd extends Msg {
  private constructor(name: string, createShellOnly: boolean) {
    super();

    if (createShellOnly) {
      return;
    }

    ten_addon.ten_nodejs_cmd_create(this, name);
  }

  static Create(name: string): Cmd {
    return new Cmd(name, false);
  }
}

ten_addon.ten_nodejs_cmd_register_class(Cmd);
