//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import z from "zod";

import { API_DESIGNER_V1, ENDPOINT_METHOD } from "@/api/endpoints/constant";
import { genResSchema } from "@/api/endpoints/utils";
import type {
  IFileContentResponse,
  IBaseDirResponse,
} from "@/types/fileSystem";

export const ENDPOINT_FILE_SYSTEM = {
  fileContent: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/file-content`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: z.object({
        file_path: z.string(),
      }),
      responseSchema: genResSchema<IFileContentResponse>(
        z.object({
          content: z.string(),
        })
      ),
    },
    [ENDPOINT_METHOD.PUT]: {
      url: `${API_DESIGNER_V1}/file-content`,
      method: ENDPOINT_METHOD.PUT,
      requestSchema: z.object({
        file_path: z.string(),
        content: z.string(),
      }),
      responseSchema: genResSchema<null>(z.null()),
    },
  },
  dirList: {
    /** @deprecated */
    [ENDPOINT_METHOD.GET]: {
      url: `${API_DESIGNER_V1}/dir-list/:path`,
      method: ENDPOINT_METHOD.GET,
      pathParams: ["path"],
      responseSchema: genResSchema<IBaseDirResponse>(
        z.object({
          entries: z.array(
            z.object({
              name: z.string(),
              path: z.string(),
              is_dir: z.boolean().optional(),
            })
          ),
        })
      ),
    },
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/dir-list`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: z.object({
        path: z.string(),
      }),
      responseSchema: genResSchema<IBaseDirResponse>(
        z.object({
          entries: z.array(
            z.object({
              name: z.string(),
              path: z.string(),
              is_dir: z.boolean().optional(),
            })
          ),
        })
      ),
    },
  },
};

export const ENDPOINT_FILE_VALIDATE = {
  property: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/property/validate`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: z.object({
        property_json_str: z.string(),
      }),
      responseSchema: z.any().optional(),
    },
  },
  manifest: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/manifest/validate`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: z.object({
        manifest_json_str: z.string(),
      }),
      responseSchema: z.any().optional(),
    },
  },
};
