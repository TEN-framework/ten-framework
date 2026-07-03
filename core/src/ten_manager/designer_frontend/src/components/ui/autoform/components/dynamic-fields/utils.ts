//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import { buildZodFieldConfig } from "@autoform/react";
import { z } from "zod";
import type { FieldTypes } from "../../auto-form";
import type { TDynamicField, TDynamicFieldType } from "../../types";

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

const fieldConfig = buildZodFieldConfig<FieldTypes>();

/**
 * Convert dynamic field values to appropriate types
 */
export const convertDynamicFieldValue = (
  type: TDynamicFieldType,
  value: unknown
): unknown => {
  switch (type) {
    case "string": {
      return typeof value === "string" ? value : String(value || "");
    }
    case "number": {
      const num = Number(value);
      return Number.isNaN(num) ? 0 : num;
    }
    case "object": {
      if (typeof value === "string") {
        try {
          return JSON.parse(value);
        } catch {
          return {};
        }
      }
      return typeof value === "object" ? value : {};
    }
    default:
      return value;
  }
};

/**
 * Create Zod schema for dynamic fields
 */
export const createDynamicFieldZodSchema = (
  field: TDynamicField
): z.ZodType => {
  switch (field.type) {
    case "string":
      return z.string().optional();
    case "number":
      return z.coerce.number().optional();
    case "object":
      return z
        .any()
        .transform((value) => {
          // If already an object (not a string), return directly
          if (typeof value === "object" && value !== null) {
            return value;
          }

          // If it's a string, try to parse as JSON
          if (typeof value === "string") {
            try {
              const parsed = JSON.parse(value);
              return parsed;
            } catch {
              // If parsing fails, return empty object
              return {};
            }
          }

          // Return empty object for other cases
          return {};
        })
        .optional();
    default:
      return z.unknown().optional();
  }
};

/**
 * Recursively converts a JSON schema property definition to a Zod schema
 */
const convertPropertyToZod = (property: TPropertyDefinition): z.ZodType => {
  // Check if this is a dynamic field
  const extendedProperty = property as TPropertyDefinition & {
    isDynamicField?: boolean;
    dynamicField?: TDynamicField;
  };

  if (extendedProperty.isDynamicField && extendedProperty.dynamicField) {
    // For dynamic fields, use our fixed schema creation logic
    return createDynamicFieldZodSchema(extendedProperty.dynamicField);
  }

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
          // Make nested properties optional by default so that
          // fields starting from the second level are not required
          // unless explicitly enforced elsewhere.
          objectSchema[key] = convertPropertyToZod(value).optional();
        }

        zodType = z.object(objectSchema);
      } else {
        // When an object has no properties, render it as an empty object
        // so the UI shows only the section title without stray inputs.
        zodType = z.object({});
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

/**
 * Enhanced conversion function with dynamic fields support
 * Converts extension property schema to Zod schema entries
 * with dynamic fields support
 */
export const convertExtensionPropertySchema2ZodSchemaWithDynamicFields = (
  input: TExtPropertySchema,
  dynamicFields: TDynamicField[] = []
) => {
  // First convert the original schema
  const schemaEntries: [string, z.ZodType][] = Object.entries(input).map(
    ([key, property]) => {
      // Check if there are dynamic fields to add to this object
      const fieldsForThisObject = dynamicFields.filter(
        (field) =>
          field.path.length === 1 &&
          field.path[0] === key &&
          property.type === "object"
      );

      if (fieldsForThisObject.length > 0 && property.type === "object") {
        // Create enhanced object schema
        const enhancedProperty = {
          ...property,
          properties: {
            ...property.properties,
            ...fieldsForThisObject.reduce((acc, field) => {
              // For dynamic fields, we need special handling
              // Don't use regular TPropertyDefinition, but mark as dynamic
              // field directly
              acc[field.key] = {
                type: field.type,
                default: field.value,
                isDynamicField: true,
                dynamicField: field,
              } as TPropertyDefinition & {
                isDynamicField: boolean;
                dynamicField: TDynamicField;
              };
              return acc;
            }, {} as TExtPropertySchema),
          },
        };
        return [key, convertPropertyToZod(enhancedProperty).optional()];
      }

      return [key, convertPropertyToZod(property).optional()];
    }
  );

  // Add root level dynamic fields
  const rootLevelDynamicFields = dynamicFields.filter(
    (field) => field.path.length === 0
  );

  for (const field of rootLevelDynamicFields) {
    schemaEntries.push([field.key, createDynamicFieldZodSchema(field)]);
  }

  return schemaEntries;
};

/**
 * Helper function: Add dynamic field to schema at specific path
 */
export const addDynamicFieldToPath = (
  schema: TExtPropertySchema,
  dynamicField: TDynamicField
): TExtPropertySchema => {
  if (dynamicField.path.length === 0) {
    // Root level field
    return {
      ...schema,
      [dynamicField.key]: {
        type: dynamicField.type,
        default: dynamicField.value,
      } as TPropertyDefinition,
    };
  }

  // Deep field - handle recursively
  const [currentPath, ...restPath] = dynamicField.path;
  const currentProperty = schema[currentPath];

  if (!currentProperty || currentProperty.type !== "object") {
    return schema; // Cannot add to non-object field
  }

  const updatedField = {
    ...dynamicField,
    path: restPath,
  };

  return {
    ...schema,
    [currentPath]: {
      ...currentProperty,
      properties: addDynamicFieldToPath(
        currentProperty.properties || {},
        updatedField
      ),
    },
  };
};
