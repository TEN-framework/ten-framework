//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import type { Extension } from "./extension.js";
import ten_addon from "./ten_addon.js";
import type { TenEnv } from "./ten_env.js";

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

  private async onDestroyInstanceProxy(
    tenEnv: TenEnv,
    instance: Extension,
    context: unknown,
  ): Promise<void> {
    ten_addon.ten_nodejs_ten_env_on_destroy_instance_done(
      tenEnv,
      instance,
      context,
    );
  }

  // This method will be called when the C addon is destroyed.
  private async onDestroy(): Promise<void> {
    // JS addon prepare to be destroyed, so notify the underlying C runtime this
    // fact.
    //
    // onDestroy() is called by the C runtime to the JS world, and then
    // immediately calls back down to the C runtime's
    // ten_nodejs_addon_on_end_of_life. It seems unnecessary to go through the
    // JS world, but it is actually needed. This is because
    // ten_nodejs_addon_on_end_of_life() internally calls NAPI's API, and
    // calling the NAPI API requires being in the JS world, hence the need for
    // this behavior of calling from the C runtime to the JS world first.
    ten_addon.ten_nodejs_addon_on_end_of_life(this);
  }

  abstract onCreateInstance(
    tenEnv: TenEnv,
    instanceName: string,
  ): Promise<Extension>;
}
