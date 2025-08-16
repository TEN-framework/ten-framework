//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import ten_addon from "../../ten_addon.js";
import { Cmd } from "./cmd.js";

export class StartGraphCmd extends Cmd {
  private constructor() {
    super("", true);

    ten_addon.ten_nodejs_cmd_start_graph_create(this);
  }

  static Create(): StartGraphCmd {
    return new StartGraphCmd();
  }

  /**
   * Set the predefined graph name for this start graph command.
   */
  setPredefinedGraphName(predefinedGraphName: string): void {
    ten_addon.ten_nodejs_cmd_start_graph_set_predefined_graph_name(
      this,
      predefinedGraphName,
    );
  }

  /**
   * Set the graph configuration from a JSON string.
   */
  setGraphFromJSON(jsonStr: string): void {
    ten_addon.ten_nodejs_cmd_start_graph_set_graph_from_json_str(this, jsonStr);
  }
}

ten_addon.ten_nodejs_cmd_start_graph_register_class(StartGraphCmd);
