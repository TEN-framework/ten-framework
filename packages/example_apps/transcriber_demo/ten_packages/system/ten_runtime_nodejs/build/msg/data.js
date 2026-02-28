//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import ten_addon from "../ten_addon.js";
import { Msg } from "./msg.js";
export class Data extends Msg {
    constructor(name, createShellOnly) {
        super();
        if (createShellOnly) {
            return;
        }
        ten_addon.ten_nodejs_data_create(this, name);
    }
    static Create(name) {
        return new Data(name, false);
    }
    allocBuf(size) {
        ten_addon.ten_nodejs_data_alloc_buf(this, size);
    }
    lockBuf() {
        return ten_addon.ten_nodejs_data_lock_buf(this);
    }
    unlockBuf(buf) {
        ten_addon.ten_nodejs_data_unlock_buf(this, buf);
    }
    getBuf() {
        return ten_addon.ten_nodejs_data_get_buf(this);
    }
}
ten_addon.ten_nodejs_data_register_class(Data);
//# sourceMappingURL=data.js.map