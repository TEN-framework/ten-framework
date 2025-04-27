//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import z from "zod";

import { API_DESIGNER_V1, ENDPOINT_METHOD } from "@/api/endpoints/constant";
import { genResSchema } from "@/api/endpoints/utils";
import { EDocLinkKey } from "@/types/doc";
import { EPreferencesLocale } from "@/types/apps";

export const ENDPOINT_DOC_LINK = {
  retrieveDocLink: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/doc-link`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: z.object({
        key: z.nativeEnum(EDocLinkKey),
        locale: z.nativeEnum(EPreferencesLocale),
      }),
      responseSchema: genResSchema(
        z.object({
          key: z.string(),
          locale: z.string(),
          text: z.string(),
        })
      ),
    },
  },
};
