//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import ten_addon from "../../ten_addon.js";
import { Msg } from "../msg.js";
export var StatusCode;
(function (StatusCode) {
    StatusCode[StatusCode["OK"] = 0] = "OK";
    StatusCode[StatusCode["ERROR"] = 1] = "ERROR";
})(StatusCode || (StatusCode = {}));
export class CmdResult extends Msg {
    constructor(statusCode, targetCmd, createShellOnly) {
        super();
        if (createShellOnly) {
            return;
        }
        ten_addon.ten_nodejs_cmd_result_create(this, statusCode, targetCmd);
    }
    static Create(statusCode, targetCmd) {
        return new CmdResult(statusCode, targetCmd, false);
    }
    getStatusCode() {
        return ten_addon.ten_nodejs_cmd_result_get_status_code(this);
    }
    setFinal(isFinal) {
        ten_addon.ten_nodejs_cmd_result_set_final(this, isFinal);
    }
    isFinal() {
        return ten_addon.ten_nodejs_cmd_result_is_final(this);
    }
    isCompleted() {
        return ten_addon.ten_nodejs_cmd_result_is_completed(this);
    }
}
ten_addon.ten_nodejs_cmd_result_register_class(CmdResult);
//# sourceMappingURL=cmd_result.js.map