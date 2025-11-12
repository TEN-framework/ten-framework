//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { buildZodFieldConfig } from "@autoform/react";
import type { FieldTypes } from "./auto-form";

export const fieldConfig = buildZodFieldConfig<
  FieldTypes,
  // {
  //   // Add types for `customData` here.
  // }
  unknown
>();
