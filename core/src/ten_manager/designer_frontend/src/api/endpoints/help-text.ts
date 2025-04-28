//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import z from "zod";

import { API_DESIGNER_V1, ENDPOINT_METHOD } from "@/api/endpoints/constant";
import { genResSchema } from "@/api/endpoints/utils";

export const ENDPOINT_HELP_TEXT = {
  helpText: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/help-text`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: z.object({
        key: z.string(),
        locale: z.string(),
      }),
      responseSchema: genResSchema<{
        key: string;
        locale: string;
        text: string;
      }>(
        z.object({
          key: z.string(),
          locale: z.string(),
          text: z.string(),
        })
      ),
    },
  },
};

export enum EHelpTextKey {
  TEN_AGENT = "ten_agent",
  TEN_FRAMEWORK = "ten_framework",
}
