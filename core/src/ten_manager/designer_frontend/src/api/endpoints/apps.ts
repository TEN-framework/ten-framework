//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import z from "zod";

import { API_DESIGNER_V1, ENDPOINT_METHOD } from "@/api/endpoints/constant";
import { genResSchema } from "@/api/endpoints/utils";
import {
  type ISetBaseDirResponse,
  type IExtensionAddon,
  type IGetAppsResponse,
  TemplatePkgsReqSchema,
  AppCreateReqSchema,
} from "@/types/apps";
import { ETenPackageType } from "@/types/extension";

export const ENDPOINT_APPS = {
  apps: {
    [ENDPOINT_METHOD.GET]: {
      url: `${API_DESIGNER_V1}/apps`,
      method: ENDPOINT_METHOD.GET,
      responseSchema: genResSchema<IGetAppsResponse>(
        z.object({
          app_info: z.array(
            z.object({
              base_dir: z.string(),
              app_uri: z.string(),
            })
          ),
        })
      ),
    },
  },
  loadApps: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/apps/load`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: z.object({
        base_dir: z.string(),
      }),
      responseSchema: genResSchema<ISetBaseDirResponse>(
        z.object({
          app_uri: z.string().nullable(),
        })
      ),
    },
  },
  reloadApps: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/apps/reload`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: z.object({
        base_dir: z.string().optional(),
      }),
      responseSchema: genResSchema(z.any()),
    },
  },
  unloadApps: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/apps/unload`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: z.object({
        base_dir: z.string().optional(),
      }),
      responseSchema: genResSchema(z.any()),
    },
  },
  appScripts: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/apps/scripts`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: z.object({
        base_dir: z.string(),
      }),
      responseSchema: genResSchema<{
        scripts?: string[] | null;
      }>(
        z.object({
          scripts: z.array(z.string()).optional().nullable(),
        })
      ),
    },
  },
  createApp: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/apps/create`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: AppCreateReqSchema,
      responseSchema: genResSchema(
        z.object({
          app_path: z.string().nullable(),
        })
      ),
    },
  },
};

export const ENDPOINT_TEMPLATES = {
  templatePkgs: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/template-pkgs`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: TemplatePkgsReqSchema,
      responseSchema: genResSchema<{
        template_name: string[];
      }>(
        z.object({
          template_name: z.array(z.string()),
        })
      ),
    },
  },
};

export const ENDPOINT_ADDONS = {
  addons: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/apps/addons`,
      method: ENDPOINT_METHOD.POST,
      body: z.object({
        base_dir: z.string().optional(),
        addon_name: z.string().optional(),
        addon_type: z.string().optional(),
      }),
      responseSchema: genResSchema<IExtensionAddon[]>(
        z.array(
          z.object({
            name: z.string(),
            url: z.string(),
            api: z.unknown().optional(),
            type: z.nativeEnum(ETenPackageType),
          })
        )
      ),
    },
  },
};
