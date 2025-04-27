//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { z } from "zod";
import { buildZodFieldConfig } from "@autoform/react";
import { FieldTypes } from "@/components/ui/autoform/AutoForm";

export type TExtPropertySchema = Record<string, Record<"type", string>>;

// const fieldConfig = buildZodFieldConfig<
//   // You should provide the "FieldTypes" type from the UI library you use
//   FieldTypes,
//   {
//     isImportant?: boolean; // You can add custom props here
//   }
// >();
const fieldConfig = buildZodFieldConfig<FieldTypes>();

export const convertExtensionPropertySchema2ZodSchema = (
  input: TExtPropertySchema
) => {
  const schemaEntries: [string, z.ZodType][] = Object.entries(input).map(
    ([key, value]) => {
      const { type } = value;
      let zodType;

      switch (type) {
        case "int64":
        case "int32":
          zodType = z.coerce
            .number()
            .superRefine(
              fieldConfig({
                inputProps: {
                  type: "number",
                  step: 1,
                },
              })
            )
            .optional();
          break;
        case "float64":
        case "float32":
          zodType = z.coerce
            .number()
            .superRefine(
              fieldConfig({
                inputProps: {
                  type: "number",
                  step: 0.1,
                },
              })
            )
            .optional();
          break;
        case "bool":
          zodType = z.boolean().optional();
          break;
        case "string":
          zodType = z.string().optional();
          break;
        default:
          zodType = z.any().optional();
      }

      return [key, zodType];
    }
  );

  return schemaEntries;
};
