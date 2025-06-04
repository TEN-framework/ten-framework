//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { TenError } from "../error/error.js";
import ten_addon from "../ten_addon.js";

export class Msg {
  getName(): string {
    return ten_addon.ten_nodejs_msg_get_name(this);
  }

  setDest(
    appUri: string | undefined = undefined,
    graphId: string | undefined = undefined,
    extension: string | undefined = undefined,
  ) {
    ten_addon.ten_nodejs_msg_set_dest(this, appUri, graphId, extension);
  }

  setPropertyFromJson(path: string, jsonStr: string): TenError | null {
    return ten_addon.ten_nodejs_msg_set_property_from_json(this, path, jsonStr);
  }

  getPropertyToJson(path: string): [string, TenError | null] {
    return ten_addon.ten_nodejs_msg_get_property_to_json(this, path);
  }

  setPropertyNumber(path: string, value: number): TenError | null {
    return ten_addon.ten_nodejs_msg_set_property_number(this, path, value);
  }

  getPropertyNumber(path: string): [number, TenError | null] {
    return ten_addon.ten_nodejs_msg_get_property_number(this, path);
  }

  setPropertyString(path: string, value: string): TenError | null {
    return ten_addon.ten_nodejs_msg_set_property_string(this, path, value);
  }

  getPropertyString(path: string): [string, TenError | null] {
    return ten_addon.ten_nodejs_msg_get_property_string(this, path);
  }

  setPropertyBool(path: string, value: boolean): TenError | null {
    return ten_addon.ten_nodejs_msg_set_property_bool(this, path, value);
  }

  getPropertyBool(path: string): [boolean, TenError | null] {
    return ten_addon.ten_nodejs_msg_get_property_bool(this, path);
  }

  setPropertyBuf(path: string, value: ArrayBuffer): TenError | null {
    return ten_addon.ten_nodejs_msg_set_property_buf(this, path, value);
  }

  getPropertyBuf(path: string): [ArrayBuffer, TenError | null] {
    return ten_addon.ten_nodejs_msg_get_property_buf(this, path);
  }
}
