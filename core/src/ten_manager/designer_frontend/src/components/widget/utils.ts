//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import { buildZodFieldConfig } from "@autoform/react";
import { z } from "zod";
import type { FieldTypes } from "@/components/ui/autoform/auto-form";

export type TExtPropertySchema = Record<string, TPropertyDefinition>;

export interface TPropertyDefinition {
  type: string;
  properties?: TExtPropertySchema;
  items?: TPropertyDefinition;
  description?: string;
  default?: unknown;
  required?: string[];
  enum?: unknown[];
  minimum?: number;
  maximum?: number;
  minLength?: number;
  maxLength?: number;
}

// const fieldConfig = buildZodFieldConfig<
//   // You should provide the "FieldTypes" type from the UI library you use
//   FieldTypes,
//   {
//     isImportant?: boolean; // You can add custom props here
//   }
// >();
const fieldConfig = buildZodFieldConfig<FieldTypes>();

/**
 * Recursively converts a JSON schema property definition to a Zod schema
 */
const convertPropertyToZod = (property: TPropertyDefinition): z.ZodType => {
  const {
    type,
    properties,
    items,
    enum: enumValues,
    minimum,
    maximum,
    minLength,
    maxLength,
  } = property;

  let zodType: z.ZodType;

  switch (type) {
    case "int64":
    case "int32":
    case "uint32":
    case "integer":
      zodType = z.coerce.number().int();
      if (typeof minimum === "number") {
        zodType = (zodType as z.ZodNumber).min(minimum);
      }
      if (typeof maximum === "number") {
        zodType = (zodType as z.ZodNumber).max(maximum);
      }
      zodType = zodType.superRefine(
        fieldConfig({
          inputProps: {
            type: "number",
            step: 1,
          },
        })
      );
      break;

    case "float64":
    case "float32":
    case "number":
      zodType = z.coerce.number();
      if (typeof minimum === "number") {
        zodType = (zodType as z.ZodNumber).min(minimum);
      }
      if (typeof maximum === "number") {
        zodType = (zodType as z.ZodNumber).max(maximum);
      }
      zodType = zodType.superRefine(
        fieldConfig({
          inputProps: {
            type: "number",
            step: 0.1,
          },
        })
      );
      break;

    case "bool":
    case "boolean":
      zodType = z.boolean();
      break;

    case "string":
      zodType = z.string();
      if (typeof minLength === "number") {
        zodType = (zodType as z.ZodString).min(minLength);
      }
      if (typeof maxLength === "number") {
        zodType = (zodType as z.ZodString).max(maxLength);
      }
      if (enumValues && Array.isArray(enumValues)) {
        zodType = z.enum(enumValues as [string, ...string[]]);
      }
      break;

    case "array":
      if (items) {
        const itemsZodType = convertPropertyToZod(items);
        zodType = z.array(itemsZodType);
      } else {
        zodType = z.array(z.any());
      }
      break;

    case "object":
      if (properties && Object.keys(properties).length > 0) {
        const objectSchema: Record<string, z.ZodType> = {};

        for (const [key, value] of Object.entries(properties)) {
          objectSchema[key] = convertPropertyToZod(value);
        }

        zodType = z.object(objectSchema);
      } else {
        zodType = z.record(z.string(), z.unknown());
      }
      break;

    default:
      console.warn(`Unknown type: ${type}, falling back to z.any()`);
      zodType = z.any();
  }

  return zodType;
};

/**
 * Converts extension property schema to Zod schema entries
 * with full recursive support
 */
export const convertExtensionPropertySchema2ZodSchema = (
  input: TExtPropertySchema
) => {
  const schemaEntries: [string, z.ZodType][] = Object.entries(input).map(
    ([key, property]) => {
      const zodType = convertPropertyToZod(property).optional();
      return [key, zodType];
    }
  );

  return schemaEntries;
};
