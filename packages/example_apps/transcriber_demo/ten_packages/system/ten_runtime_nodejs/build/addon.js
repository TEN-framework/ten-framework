import ten_addon from "./ten_addon.js";
export class Addon {
    constructor() {
        ten_addon.ten_nodejs_addon_create(this);
    }
    async onCreateInstanceProxy(tenEnv, instanceName, context) {
        const extension = await this.onCreateInstance(tenEnv, instanceName);
        ten_addon.ten_nodejs_ten_env_on_create_instance_done(tenEnv, extension, context);
    }
    async onDestroyInstanceProxy(tenEnv, instance, context) {
        ten_addon.ten_nodejs_ten_env_on_destroy_instance_done(tenEnv, instance, context);
    }
    // This method will be called when the C addon is destroyed.
    async onDestroy() {
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
}
//# sourceMappingURL=addon.js.map