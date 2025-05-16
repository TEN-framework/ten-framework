//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { Extension } from "../extension/extension.js";
import ten_addon from "../ten_addon.js";
import { TenEnv } from "../ten_env/ten_env.js";

export abstract class Addon {
  constructor() {
    ten_addon.ten_nodejs_addon_create(this);
  }

  private async onCreateInstanceProxy(
    tenEnv: TenEnv,
    instanceName: string,
    context: unknown,
  ): Promise<void> {
    const extension = await this.onCreateInstance(tenEnv, instanceName);

    ten_addon.ten_nodejs_ten_env_on_create_instance_done(
      tenEnv,
      extension,
      context,
    );
  }

  // This method will be called when the C addon is destroyed.
  private async onDestroy(): Promise<void> {
    // JS addon prepare to be destroyed, so notify the underlying C runtime this
    // fact.
    ten_addon.ten_nodejs_addon_on_end_of_life(this);
  }

  abstract onCreateInstance(
    tenEnv: TenEnv,
    instanceName: string,
  ): Promise<Extension>;
}
