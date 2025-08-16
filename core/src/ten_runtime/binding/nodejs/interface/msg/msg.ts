//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import type { TenError } from "../error.js";
import { Loc } from "../loc.js";
import ten_addon from "../ten_addon.js";

export class Msg {
  getName(): string {
    return ten_addon.ten_nodejs_msg_get_name(this);
  }

  getSource(): Loc {
    const arr = ten_addon.ten_nodejs_msg_get_source(this);
    return new Loc({
      appUri: arr[0],
      graphId: arr[1],
      extensionName: arr[2],
    });
  }

  setDests(dests: Partial<Loc>[]): TenError | undefined {
    const locs: Loc[] = dests.map((d) => new Loc(d));
    return ten_addon.ten_nodejs_msg_set_dests(this, locs);
  }

  setPropertyFromJson(path: string, jsonStr: string): TenError | undefined {
    return ten_addon.ten_nodejs_msg_set_property_from_json(this, path, jsonStr);
  }

  getPropertyToJson(path: string): [string, TenError | undefined] {
    return ten_addon.ten_nodejs_msg_get_property_to_json(this, path);
  }

  setPropertyNumber(path: string, value: number): TenError | undefined {
    return ten_addon.ten_nodejs_msg_set_property_number(this, path, value);
  }

  getPropertyNumber(path: string): [number, TenError | undefined] {
    return ten_addon.ten_nodejs_msg_get_property_number(this, path);
  }

  setPropertyString(path: string, value: string): TenError | undefined {
    return ten_addon.ten_nodejs_msg_set_property_string(this, path, value);
  }

  getPropertyString(path: string): [string, TenError | undefined] {
    return ten_addon.ten_nodejs_msg_get_property_string(this, path);
  }

  setPropertyBool(path: string, value: boolean): TenError | undefined {
    return ten_addon.ten_nodejs_msg_set_property_bool(this, path, value);
  }

  getPropertyBool(path: string): [boolean, TenError | undefined] {
    return ten_addon.ten_nodejs_msg_get_property_bool(this, path);
  }

  setPropertyBuf(path: string, value: ArrayBuffer): TenError | undefined {
    return ten_addon.ten_nodejs_msg_set_property_buf(this, path, value);
  }

  getPropertyBuf(path: string): [ArrayBuffer, TenError | undefined] {
    return ten_addon.ten_nodejs_msg_get_property_buf(this, path);
  }
}
