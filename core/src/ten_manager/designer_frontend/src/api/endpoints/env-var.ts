//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import { z } from "zod";
import { API_DESIGNER_V1, ENDPOINT_METHOD } from "./constant";

export const ENDPOINT_ENV_VAR = {
  getEnvVar: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/env-var`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: z.object({
        name: z.string(),
      }),
      responseSchema: z.object({
        value: z.string(),
      }),
    },
  },
};
