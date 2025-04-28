//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { z } from "zod";

export function getZodDefaults<Schema extends z.AnyZodObject>(schema: Schema) {
  return Object.fromEntries(
    Object.entries(schema.shape).map(([key, value]) => {
      if (value instanceof z.ZodDefault)
        return [key, value._def.defaultValue()];
      return [key, undefined];
    })
  );
}

// example usage:
// stringToJSONSchema.pipe(
//   z.object({ id: z.string(), redirect_uri: z.string() })
// )
export const stringToJSONSchema = z.string().transform((str, ctx) => {
  try {
    return JSON.parse(str);
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
  } catch (e) {
    ctx.addIssue({ code: "custom", message: "Invalid JSON" });
    return z.NEVER;
  }
});

export const getCurrentWindowSize = () => {
  if (typeof window !== "undefined") {
    return {
      width: window.innerWidth,
      height: window.innerHeight,
    };
  }
};
