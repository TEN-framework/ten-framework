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
  AddConnectionPayloadSchema,
  AddNodePayloadSchema,
  DeleteConnectionPayloadSchema,
  DeleteNodePayloadSchema,
  GraphInfo,
  GraphUiNodeGeometrySchema,
  SetGraphUiPayloadSchema,
  UpdateNodePropertyPayloadSchema,
} from "@/types/graphs";

export const ENDPOINT_GRAPHS = {
  addNode: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/graphs/nodes/add`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: AddNodePayloadSchema,
      responseSchema: genResSchema(
        z.object({
          success: z.boolean(),
        })
      ),
    },
  },
  deleteNode: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/graphs/nodes/delete`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: DeleteNodePayloadSchema,
      responseSchema: genResSchema(
        z.object({
          success: z.boolean(),
        })
      ),
    },
  },
  nodesPropertyUpdate: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/graphs/nodes/property/update`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: UpdateNodePropertyPayloadSchema,
      responseSchema: genResSchema(z.any()), // TODO: add response schema
    },
  },
  replaceNode: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/graphs/nodes/replace`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: AddNodePayloadSchema,
      responseSchema: genResSchema(
        z.object({
          success: z.boolean(),
        })
      ),
    },
  },
  addConnection: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/graphs/connections/add`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: AddConnectionPayloadSchema,
      responseSchema: genResSchema(
        z
          .looseObject({
            success: z.boolean(),
          })
          .or(z.any())
      ),
    },
  },
  deleteConnection: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/graphs/connections/delete`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: DeleteConnectionPayloadSchema,
      responseSchema: genResSchema(z.any()),
    },
  },
  graphs: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/graphs`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: z.object({}),
      responseSchema: genResSchema<GraphInfo[]>(z.array(GraphInfo)),
    },
  },
  graphsAutoStart: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/graphs/auto-start`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: z.object({
        graph_id: z.string(),
        auto_start: z.boolean(),
      }),
      responseSchema: genResSchema(
        z.object({
          success: z.boolean(),
        })
      ),
    },
  },
};

/**
 * @deprecated
 * This endpoint is deprecated and will be removed in the future.
 * Use `persistentSchema` endpoint instead.
 */
export const ENDPOINT_GRAPH_UI = {
  set: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/internal-config/graph-ui/set`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: SetGraphUiPayloadSchema,
      responseSchema: genResSchema(z.any()), // TODO: add response schema
    },
  },
  get: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/internal-config/graph-ui/get`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: z.object({
        graph_id: z.string(),
      }),
      responseSchema: genResSchema(
        z.union([
          z.object({
            graph_geometry: z.object({
              nodes_geometry: z.array(GraphUiNodeGeometrySchema),
            }),
          }),
          z.any(),
        ])
      ),
    },
  },
};
