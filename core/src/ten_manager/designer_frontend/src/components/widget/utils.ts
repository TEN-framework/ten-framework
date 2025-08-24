//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

// Import shared utilities from dynamic-fields
import {
  addDynamicFieldToPath,
  convertDynamicFieldValue,
  convertExtensionPropertySchema2ZodSchema,
  convertExtensionPropertySchema2ZodSchemaWithDynamicFields,
  createDynamicFieldZodSchema,
  type TExtPropertySchema,
  type TPropertyDefinition,
} from "@/components/ui/autoform/components/dynamic-fields/utils";

export type { TExtPropertySchema, TPropertyDefinition };

// Re-export utilities for backward compatibility
export {
  convertDynamicFieldValue,
  createDynamicFieldZodSchema,
  convertExtensionPropertySchema2ZodSchema,
  convertExtensionPropertySchema2ZodSchemaWithDynamicFields,
  addDynamicFieldToPath,
};
