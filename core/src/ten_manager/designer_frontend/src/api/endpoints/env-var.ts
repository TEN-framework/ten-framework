import { API_DESIGNER_V1, ENDPOINT_METHOD } from "./constant";
import { z } from "zod";

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
      })
    }
  }
};