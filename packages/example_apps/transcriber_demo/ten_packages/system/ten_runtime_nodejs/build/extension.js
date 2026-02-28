import ten_addon from "./ten_addon.js";
export class Extension {
    constructor(name) {
        ten_addon.ten_nodejs_extension_create(this, name);
    }
    async onConfigureProxy(tenEnv) {
        await this.onConfigure(tenEnv);
        ten_addon.ten_nodejs_ten_env_on_configure_done(tenEnv);
    }
    async onInitProxy(tenEnv) {
        await this.onInit(tenEnv);
        ten_addon.ten_nodejs_ten_env_on_init_done(tenEnv);
    }
    async onStartProxy(tenEnv) {
        await this.onStart(tenEnv);
        ten_addon.ten_nodejs_ten_env_on_start_done(tenEnv);
    }
    async onStopProxy(tenEnv) {
        await this.onStop(tenEnv);
        ten_addon.ten_nodejs_ten_env_on_stop_done(tenEnv);
    }
    async onDeinitProxy(tenEnv) {
        await this.onDeinit(tenEnv);
        ten_addon.ten_nodejs_ten_env_on_deinit_done(tenEnv);
    }
    async onCmdProxy(tenEnv, cmd) {
        await this.onCmd(tenEnv, cmd);
    }
    async onDataProxy(tenEnv, data) {
        await this.onData(tenEnv, data);
    }
    async onAudioFrameProxy(tenEnv, frame) {
        await this.onAudioFrame(tenEnv, frame);
    }
    async onVideoFrameProxy(tenEnv, frame) {
        await this.onVideoFrame(tenEnv, frame);
    }
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    async onConfigure(tenEnv) {
        // stub for override
    }
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    async onInit(tenEnv) {
        // stub for override
    }
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    async onStart(tenEnv) {
        // stub for override
    }
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    async onStop(tenEnv) {
        // stub for override
    }
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    async onDeinit(tenEnv) {
        // stub for override
    }
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    async onCmd(tenEnv, cmd) {
        // stub for override
    }
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    async onData(tenEnv, data) {
        // stub for override
    }
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    async onAudioFrame(tenEnv, frame) {
        // stub for override
    }
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    async onVideoFrame(tenEnv, frame) {
        // stub for override
    }
}
//# sourceMappingURL=extension.js.map