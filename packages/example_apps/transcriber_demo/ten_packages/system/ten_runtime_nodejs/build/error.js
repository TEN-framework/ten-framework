//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import ten_addon from "./ten_addon.js";
export var TenErrorCode;
(function (TenErrorCode) {
    // ErrorCodeGeneric is the default errno, for those users only care error
    // msgs.
    TenErrorCode[TenErrorCode["ErrorCodeGeneric"] = 1] = "ErrorCodeGeneric";
    // ErrorCodeInvalidJSON means the json data is invalid.
    TenErrorCode[TenErrorCode["ErrorCodeInvalidJSON"] = 2] = "ErrorCodeInvalidJSON";
    // ErrorCodeInvalidArgument means invalid parameter.
    TenErrorCode[TenErrorCode["ErrorCodeInvalidArgument"] = 3] = "ErrorCodeInvalidArgument";
    // ErrorCodeInvalidType means invalid type.
    TenErrorCode[TenErrorCode["ErrorCodeInvalidType"] = 4] = "ErrorCodeInvalidType";
    // ErrorCodeInvalidGraph means invalid graph.
    TenErrorCode[TenErrorCode["ErrorCodeInvalidGraph"] = 5] = "ErrorCodeInvalidGraph";
    // ErrorCodeTenIsClosed means the TEN world is closed.
    TenErrorCode[TenErrorCode["ErrorCodeTenIsClosed"] = 6] = "ErrorCodeTenIsClosed";
    // ErrorCodeMsgNotConnected means the msg is not connected in the graph.
    TenErrorCode[TenErrorCode["ErrorCodeMsgNotConnected"] = 7] = "ErrorCodeMsgNotConnected";
    // ErrorCodeTimeout means timed out.
    TenErrorCode[TenErrorCode["ErrorCodeTimeout"] = 8] = "ErrorCodeTimeout";
})(TenErrorCode || (TenErrorCode = {}));
export class TenError {
    _errorCode;
    _errorMessage;
    constructor(errorCode, errorMessage) {
        this._errorCode = errorCode;
        this._errorMessage = errorMessage;
    }
    get errorCode() {
        return this._errorCode;
    }
    get errorMessage() {
        return this._errorMessage;
    }
}
ten_addon.ten_nodejs_error_register_class(TenError);
//# sourceMappingURL=error.js.map