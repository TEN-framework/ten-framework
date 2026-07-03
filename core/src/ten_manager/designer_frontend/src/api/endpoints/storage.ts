//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import z from "zod";

import { API_DESIGNER_V1, ENDPOINT_METHOD } from "@/api/endpoints/constant";
import { genResSchema } from "@/api/endpoints/utils";

export const ENDPOINT_PREFERENCES = {
  logviewer_line_size: {
    [ENDPOINT_METHOD.GET]: {
      url: `${API_DESIGNER_V1}/preferences/logviewer_line_size`,
      method: ENDPOINT_METHOD.GET,
      responseSchema: genResSchema<{
        logviewer_line_size: number;
      }>(
        z.object({
          logviewer_line_size: z.number(),
        })
      ),
    },
    [ENDPOINT_METHOD.PUT]: {
      url: `${API_DESIGNER_V1}/preferences/logviewer_line_size`,
      method: ENDPOINT_METHOD.PUT,
      requestSchema: z.object({
        logviewer_line_size: z.number().min(1),
      }),
      responseSchema: genResSchema<{
        logviewer_line_size: number;
      }>(
        z.object({
          logviewer_line_size: z.number(),
        })
      ),
    },
  },
};

export const ENDPOINT_STORAGE = {
  inMemoryGet: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/storage/in-memory/get`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: z.object({
        key: z.string(),
      }),
      responseSchema: genResSchema<{ value?: unknown }>(
        z.object({
          value: z.unknown(),
        })
      ),
    },
  },
  inMemorySet: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/storage/in-memory/set`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: z.object({
        key: z.string(),
        value: z.any(),
      }),
      responseSchema: genResSchema<{ success: boolean }>(
        z.object({
          success: z.boolean(),
        })
      ),
    },
  },
  persistentGet: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/storage/persistent/get`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: z.object({
        key: z.string(),
      }),
      responseSchema: genResSchema<{ value?: unknown }>(
        z.object({
          value: z.any(),
        })
      ),
    },
  },
  persistentSet: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/storage/persistent/set`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: z.object({
        key: z.string(),
        value: z.unknown(),
      }),
      responseSchema: genResSchema<{ success: boolean }>(
        z.object({
          success: z.boolean(),
        })
      ),
    },
  },
  persistentSchema: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/storage/persistent/schema`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: z.object({
        schema: z.any(), // Schema can be any valid JSON schema
      }),
      responseSchema: genResSchema<{
        success: boolean;
        cleaned_fields?: string[];
      }>(
        z.object({
          success: z.boolean(),
          cleaned_fields: z.array(z.string()).optional(),
        })
      ),
    },
  },
};
