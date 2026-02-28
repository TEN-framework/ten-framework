import type { Addon } from "./addon.js";
type Ctor<T> = {
    new (): T;
    prototype: T;
};
type addonRegisterHandler = (registerContext: unknown) => void;
export declare class AddonManager {
    private static _instance;
    private _registry;
    private _registeredAddons;
    private constructor();
    private static findAppBaseDir;
    static getInstance(): AddonManager;
    setRegisterHandler(name: string, handler: addonRegisterHandler): void;
    loadAllAddons(): Promise<void>;
    loadSingleAddon(name: string): Promise<boolean>;
    deinit(): void;
    private registerSingleAddon;
}
export declare function RegisterAddonAsExtension(name: string): <T extends Ctor<Addon>>(klass: T) => T;
export {};
