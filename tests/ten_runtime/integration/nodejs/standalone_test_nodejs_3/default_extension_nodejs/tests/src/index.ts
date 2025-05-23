//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { AddonManager, App, TenEnv } from "ten-runtime-nodejs";
import { GreetingTester } from "./greeting.js";
import { AudioFrameTester, CmdTester, DataTester, VideoFrameTester } from "./basic_msg.js";

let fakeApp: FakeApp;
let fakeAppRunPromise: Promise<void>;
const test_addon_name = "default_extension_nodejs";

class FakeApp extends App {
    private initPromise: Promise<void>;
    private resolveInit: (() => void) | null = null;

    constructor() {
        super();
        this.initPromise = new Promise((resolve) => {
            this.resolveInit = resolve;
        });
    }

    async onInit(_tenEnv: TenEnv): Promise<void> {
        console.log("Default App onInit");
        if (this.resolveInit) {
            this.resolveInit();
        }
    }

    async waitForInit(): Promise<void> {
        return this.initPromise;
    }
}

async function main() {
    // SETUP
    await AddonManager.getInstance().loadAllAddons();

    fakeApp = new FakeApp();
    fakeAppRunPromise = fakeApp.run();

    // wait for the app to be initialized
    await fakeApp.waitForInit();
    // END OF SETUP

    // TEST BODY

    const greetingMsg = "Hello, world!";
    const greetingTester = new GreetingTester(greetingMsg);
    greetingTester.setTestModeSingle(test_addon_name, `{"greetingMsg": "${greetingMsg}"}`);
    await greetingTester.run();

    const cmdTester = new CmdTester();
    cmdTester.setTestModeSingle(test_addon_name, "{}");
    await cmdTester.run();

    const dataTester = new DataTester();
    dataTester.setTestModeSingle(test_addon_name, "{}");
    await dataTester.run();

    const videoFrameTester = new VideoFrameTester();
    videoFrameTester.setTestModeSingle(test_addon_name, "{}");
    await videoFrameTester.run();

    const audioFrameTester = new AudioFrameTester();
    audioFrameTester.setTestModeSingle(test_addon_name, "{}");
    await audioFrameTester.run();

    console.log("All tests passed");

    // END OF TEST BODY

    // TEARDOWN
    fakeApp.close();
    await fakeAppRunPromise;

    (global as unknown as { gc: () => void }).gc();
    // END OF TEARDOWN
}

main();
