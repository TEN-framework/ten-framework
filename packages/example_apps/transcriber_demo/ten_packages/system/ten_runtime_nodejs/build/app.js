//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { AddonManager } from "./addon_manager.js";
import ten_addon from "./ten_addon.js";
export class App {
    constructor() {
        ten_addon.ten_nodejs_app_create(this);
    }
    async onConfigureProxy(tenEnv) {
        await this.onConfigure(tenEnv);
        const addonManager = AddonManager.getInstance();
        await addonManager.loadAllAddons();
        ten_addon.ten_nodejs_ten_env_on_configure_done(tenEnv);
    }
    async onInitProxy(tenEnv) {
        await this.onInit(tenEnv);
        ten_addon.ten_nodejs_ten_env_on_init_done(tenEnv);
    }
    async onDeinitProxy(tenEnv) {
        await this.onDeinit(tenEnv);
        const addonManager = AddonManager.getInstance();
        addonManager.deinit();
        ten_addon.ten_nodejs_ten_env_on_deinit_done(tenEnv);
        // JS app prepare to be destroyed, so notify the underlying C runtime this
        // fact.
        ten_addon.ten_nodejs_app_on_end_of_life(this);
        global.gc();
    }
    // The ten app should be run in another native thread not the JS main thread.
    async run() {
        await ten_addon.ten_nodejs_app_run(this);
    }
    async close() {
        ten_addon.ten_nodejs_app_close(this);
    }
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    async onConfigure(tenEnv) {
        // Stub for override.
    }
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    async onInit(tenEnv) {
        // Stub for override.
    }
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    async onDeinit(tenEnv) {
        // Stub for override.
    }
}
//# sourceMappingURL=app.js.map