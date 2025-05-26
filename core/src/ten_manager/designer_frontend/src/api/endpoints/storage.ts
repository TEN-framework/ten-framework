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
