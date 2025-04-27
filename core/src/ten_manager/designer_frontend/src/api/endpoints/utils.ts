//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import z from "zod";

export const RESPONSE_COMMON_SCHEMA = z.object({
  status: z.string(),
  data: z.any(),
});

export const genResSchema = <T>(schema: z.ZodType<T>) => {
  return RESPONSE_COMMON_SCHEMA.extend({
    data: schema,
  });
};
