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
  MsgCompatiblePayloadSchema,
  MsgCompatibleResponseItemSchema,
} from "@/types/graphs";

export const ENDPOINT_MESSAGES = {
  compatible: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/messages/compatible`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: MsgCompatiblePayloadSchema,
      responseSchema: genResSchema(z.array(MsgCompatibleResponseItemSchema)),
    },
  },
};
