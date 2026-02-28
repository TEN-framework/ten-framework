import type { TenEnv } from "./ten_env.js";
export declare class App {
    constructor();
    private onConfigureProxy;
    private onInitProxy;
    private onDeinitProxy;
    run(): Promise<void>;
    close(): Promise<void>;
    onConfigure(tenEnv: TenEnv): Promise<void>;
    onInit(tenEnv: TenEnv): Promise<void>;
    onDeinit(tenEnv: TenEnv): Promise<void>;
}
