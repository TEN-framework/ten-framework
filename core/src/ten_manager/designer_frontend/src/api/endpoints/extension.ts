//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import z from "zod";

import { API_DESIGNER_V1, ENDPOINT_METHOD } from "@/api/endpoints/constant";
import { TenCloudStorePackageSchema } from "@/types/extension";
import { genResSchema } from "@/api/endpoints/utils";
import { ExtensionSchema } from "@/types/extension";

export const ENDPOINT_EXTENSION = {
  registryPackages: {
    [ENDPOINT_METHOD.GET]: {
      url: `${API_DESIGNER_V1}/registry/packages`,
      method: ENDPOINT_METHOD.GET,
      queryParams: ["page", "pageSize"],
      responseSchema: z.object({
        status: z.string(),
        meta: z.any(),
        data: z.object({
          totalSize: z.number(),
          packages: z.array(TenCloudStorePackageSchema),
        }),
      }),
    },
  },
  schema: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/extensions/schema`,
      method: ENDPOINT_METHOD.POST,
      requestPayload: z.object({
        app_base_dir: z.string(),
        addon_name: z.string(),
      }),
      responseSchema: genResSchema(
        z.object({
          schema: ExtensionSchema,
        })
      ),
    },
  },
  getProperty: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/extensions/property/get`,
      method: ENDPOINT_METHOD.POST,
      requestPayload: z.object({
        app_base_dir: z.string(),
        addon_name: z.string(),
      }),
      responseSchema: genResSchema(
        z.object({
          property: z.record(z.string(), z.any()).nullable().optional(),
        })
      ),
    },
  },
};
