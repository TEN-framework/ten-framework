//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import type { FieldError, FieldErrors, FieldValues } from "react-hook-form";
//
// For zodResolver in @hookform/resolvers/zod currently not support zod/v4
// so we need to implement our own resolver
//
import type { ZodError, ZodType } from "zod/v4";

// Utility to convert ZodError to Hook Form-compatible FieldErrors
const zodToHookFormErrors = (zodError: ZodError): FieldErrors => {
  const errors: FieldErrors = {};

  for (const issue of zodError.issues) {
    const path = issue.path.join(".") || "root";
    errors[path] = {
      type: issue.code,
      message: issue.message,
    } as FieldError;
  }

  return errors;
};

// TODO - remove this when @hookform/resolvers/zod support zod/v4
// import { zodResolver } from "@hookform/resolvers/zod";
// Custom resolver for useForm()
export const customResolver = (schema: ZodType) => {
  return async (
    values: FieldValues
  ): Promise<{
    values: FieldValues;
    errors: FieldErrors;
  }> => {
    try {
      const result = await schema.safeParseAsync(values);

      if (result.success) {
        return {
          values: result.data as FieldValues,
          errors: {},
        };
      } else {
        return {
          values: {},
          errors: zodToHookFormErrors(result.error),
        };
      }
    } catch (error) {
      console.error("Resolver error: ", error);
      return {
        values: {},
        errors: {
          root: {
            type: "unknown",
            message: "An unknown error occurred during validation",
          } as FieldError,
        },
      };
    }
  };
};
