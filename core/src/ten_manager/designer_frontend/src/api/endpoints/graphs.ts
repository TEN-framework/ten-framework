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
  GraphUiNodeGeometrySchema,
  type IGraph,
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
      responseSchema: genResSchema(z.any()), // TODO: add response schema
    },
  },
  deleteConnection: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/graphs/connections/delete`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: DeleteConnectionPayloadSchema,
      responseSchema: genResSchema(z.any()), // TODO: add response schema
    },
  },
  graphs: {
    [ENDPOINT_METHOD.POST]: {
      url: `${API_DESIGNER_V1}/graphs`,
      method: ENDPOINT_METHOD.POST,
      requestSchema: z.object({}),
      responseSchema: genResSchema<IGraph[]>(
        z.array(
          z.object({
            graph_id: z.string(),
            name: z.string().nullable(),
            auto_start: z.boolean().nullable(),
            base_dir: z.string().nullable(),
            graph: z.object({
              nodes: z.array(
                z.object({
                  addon: z.string(),
                  name: z.string(),
                  extension_group: z.string().optional(),
                  app: z.string().optional(),
                  property: z.unknown().optional(),
                  api: z.unknown().optional(),
                  is_installed: z.boolean(),
                })
              ),
              connections: z.array(z.unknown()),
              exposed_messages: z.array(z.unknown()),
              exposed_properties: z.array(z.unknown()),
            }),
          })
        ) as z.ZodType<IGraph[]>
      ),
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
