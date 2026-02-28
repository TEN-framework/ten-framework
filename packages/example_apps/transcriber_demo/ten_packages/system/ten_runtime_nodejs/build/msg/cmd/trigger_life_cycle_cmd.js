//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import ten_addon from "../../ten_addon.js";
import { Cmd } from "./cmd.js";
export class TriggerLifeCycleCmd extends Cmd {
    constructor() {
        super("", true);
        ten_addon.ten_nodejs_cmd_trigger_life_cycle_create(this);
    }
    static Create() {
        return new TriggerLifeCycleCmd();
    }
    /**
     * Set the stage for this trigger life cycle command.
     * Valid values are "start" and "stop".
     */
    setStage(stage) {
        ten_addon.ten_nodejs_cmd_trigger_life_cycle_set_stage(this, stage);
    }
}
ten_addon.ten_nodejs_cmd_trigger_life_cycle_register_class(TriggerLifeCycleCmd);
//# sourceMappingURL=trigger_life_cycle_cmd.js.map