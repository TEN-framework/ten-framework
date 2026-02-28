import type { Extension } from "./extension.js";
import type { TenEnv } from "./ten_env.js";
export declare abstract class Addon {
    constructor();
    private onCreateInstanceProxy;
    private onDestroyInstanceProxy;
    private onDestroy;
    abstract onCreateInstance(tenEnv: TenEnv, instanceName: string): Promise<Extension>;
}
