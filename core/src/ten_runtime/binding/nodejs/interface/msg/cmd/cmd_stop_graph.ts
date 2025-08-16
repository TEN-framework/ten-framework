//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import ten_addon from "../../ten_addon.js";
import { Cmd } from "./cmd.js";

export class StopGraphCmd extends Cmd {
  private constructor() {
    super("", true);

    ten_addon.ten_nodejs_cmd_stop_graph_create(this);
  }

  static Create(): StopGraphCmd {
    return new StopGraphCmd();
  }
}

ten_addon.ten_nodejs_cmd_stop_graph_register_class(StopGraphCmd);
