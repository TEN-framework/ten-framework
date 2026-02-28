import ten_addon from "./ten_addon.js";
export class ExtensionTester {
    constructor() {
        ten_addon.ten_nodejs_extension_tester_create(this);
    }
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    async onStart(tenEnvTester) {
        // Stub for override.
    }
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    async onStop(tenEnvTester) {
        // Stub for override.
    }
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    async onDeinit(tenEnvTester) {
        // Stub for override.
    }
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    async onCmd(tenEnvTester, cmd) {
        // Stub for override.
    }
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    async onData(tenEnvTester, data) {
        // Stub for override.
    }
    async onAudioFrame(
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    tenEnvTester, 
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    audioFrame) {
        // Stub for override.
    }
    async onVideoFrame(
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    tenEnvTester, 
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    videoFrame) {
        // Stub for override.
    }
    async run() {
        return ten_addon.ten_nodejs_extension_tester_run(this);
    }
    setTestModeSingle(addonName, propertyJsonStr) {
        return ten_addon.ten_nodejs_extension_tester_set_test_mode_single(this, addonName, propertyJsonStr);
    }
    setTimeout(usec) {
        return ten_addon.ten_nodejs_extension_tester_set_timeout(this, usec);
    }
    async onInitProxy(tenEnvTester) {
        ten_addon.ten_nodejs_ten_env_tester_on_init_done(tenEnvTester);
    }
    async onStartProxy(tenEnvTester) {
        await this.onStart(tenEnvTester);
        ten_addon.ten_nodejs_ten_env_tester_on_start_done(tenEnvTester);
    }
    async onStopProxy(tenEnvTester) {
        await this.onStop(tenEnvTester);
        ten_addon.ten_nodejs_ten_env_tester_on_stop_done(tenEnvTester);
    }
    async onDeinitProxy(tenEnvTester) {
        await this.onDeinit(tenEnvTester);
        ten_addon.ten_nodejs_ten_env_tester_on_deinit_done(tenEnvTester);
        // JS extension_tester prepare to be destroyed, so notify the underlying C
        // runtime this fact.
        ten_addon.ten_nodejs_extension_tester_on_end_of_life(this);
        global.gc();
    }
    async onCmdProxy(tenEnvTester, cmd) {
        await this.onCmd(tenEnvTester, cmd);
    }
    async onDataProxy(tenEnvTester, data) {
        await this.onData(tenEnvTester, data);
    }
    async onAudioFrameProxy(tenEnvTester, audioFrame) {
        await this.onAudioFrame(tenEnvTester, audioFrame);
    }
    async onVideoFrameProxy(tenEnvTester, videoFrame) {
        await this.onVideoFrame(tenEnvTester, videoFrame);
    }
}
//# sourceMappingURL=extension_tester.js.map