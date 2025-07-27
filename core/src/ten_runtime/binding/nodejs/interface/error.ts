//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import ten_addon from "./ten_addon.js";

export enum TenErrorCode {
  // ErrorCodeGeneric is the default errno, for those users only care error
  // msgs.
  ErrorCodeGeneric = 1,

  // ErrorCodeInvalidJSON means the json data is invalid.
  ErrorCodeInvalidJSON = 2,

  // ErrorCodeInvalidArgument means invalid parameter.
  ErrorCodeInvalidArgument = 3,

  // ErrorCodeInvalidType means invalid type.
  ErrorCodeInvalidType = 4,

  // ErrorCodeInvalidGraph means invalid graph.
  ErrorCodeInvalidGraph = 5,

  // ErrorCodeTenIsClosed means the TEN world is closed.
  ErrorCodeTenIsClosed = 6,

  // ErrorCodeMsgNotConnected means the msg is not connected in the graph.
  ErrorCodeMsgNotConnected = 7,

  // ErrorCodeTimeout means timed out.
  ErrorCodeTimeout = 8,
}

export class TenError {
  private _errorCode: TenErrorCode;
  private _errorMessage: string;

  constructor(errorCode: TenErrorCode, errorMessage: string) {
    this._errorCode = errorCode;
    this._errorMessage = errorMessage;
  }

  get errorCode(): TenErrorCode {
    return this._errorCode;
  }

  get errorMessage(): string | undefined {
    return this._errorMessage;
  }
}

ten_addon.ten_nodejs_error_register_class(TenError);
