//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import type { ExtendableAutoFormProps } from "@autoform/react";
import type { FieldValues } from "react-hook-form";
import type { TExtPropertySchema } from "@/components/widget/utils";

export type TDynamicFieldType = "string" | "number" | "object";

export interface TDynamicField {
  key: string;
  type: TDynamicFieldType;
  value: unknown;
  // Field path, e.g., [] for root level, ["params"] for under params
  path: string[];
}

export interface AutoFormProps<T extends FieldValues>
  extends ExtendableAutoFormProps<T> {
  allowDynamicFields?: boolean;
  onDynamicFieldsChange?: (fields: TDynamicField[]) => void;
  dynamicFieldsTitle?: string;
  // 原始 schema 用于内部构建增强的 schema
  originalSchema?: TExtPropertySchema;
}
