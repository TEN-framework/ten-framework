//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { z } from "zod";

import { stringToJSONSchema } from "@/utils";

/** @deprecated */
export interface IBackendNode {
  addon: string;
  name: string;
  extension_group?: string;
  app?: string;
  property?: Record<string, unknown> | null;
  api?: unknown;
  is_installed: boolean;
  type: "extension";
}

export const BackendNodeExtension = z.object({
  addon: z.string(),
  name: z.string(),
  extension_group: z.string().optional(),
  app: z.string().optional(),
  property: z.object(z.unknown()).optional(),
  api: z.unknown().optional(),
  is_installed: z.boolean(),
  type: z.literal("extension"),
});
export type BackendNodeExtension = z.infer<typeof BackendNodeExtension>;

export const BackendNodeSelector = z.object({
  name: z.string(),
  type: z.literal("selector"),
  filter: z.any().optional(), // todo: selector&subgraph
});
export type BackendNodeSelector = z.infer<typeof BackendNodeSelector>;

export const BackendNodeSubGraph = z.looseObject({
  type: z.literal("subgraph"),
}); // todo: selector&subgraph
export type BackendNodeSubGraph = z.infer<typeof BackendNodeSubGraph>;

export enum EConnectionType {
  CMD = "cmd",
  DATA = "data",
  AUDIO_FRAME = "audio_frame",
  VIDEO_FRAME = "video_frame",
}

export interface IBackendConnection {
  app?: string;
  extension: string;
  [EConnectionType.CMD]?: {
    name: string;
    source?: {
      app?: string;
      extension: string;
    }[];
    dest?: {
      app?: string;
      extension: string;
    }[];
  }[];
  [EConnectionType.DATA]?: {
    name: string;
    source?: {
      app?: string;
      extension: string;
    }[];
    dest: {
      app?: string;
      extension: string;
    }[];
  }[];
  [EConnectionType.AUDIO_FRAME]?: {
    name: string;
    source?: {
      app?: string;
      extension: string;
    }[];
    dest: {
      app?: string;
      extension: string;
    }[];
  }[];
  [EConnectionType.VIDEO_FRAME]?: {
    name: string;
    source?: {
      app?: string;
      extension: string;
    }[];
    dest: {
      app?: string;
      extension: string;
    }[];
  }[];
}

export interface IGraph {
  graph_id: string;
  name?: string;
  auto_start?: boolean;
  base_dir?: string;
  graph: {
    // eslint-disable-next-line max-len
    // nodes: (BackendNodeExtension | BackendNodeSelector | BackendNodeSubGraph)[];
    nodes: BackendNodeExtension[]; // todo: selector&subgraph
    connections: IBackendConnection[];
    exposed_messages: Array<{
      type: string;
      name: string;
      extension?: string;
      subgraph?: string;
    }>;
    exposed_properties: Array<{
      extension?: string;
      subgraph?: string;
      name: string;
    }>;
  };
}

export type TConnectionItem = {
  name: string;
  srcApp: string;
  destApp: string;
};

export type TConnectionMap = Record<string, Set<TConnectionItem>>;

export enum EGraphActions {
  ADD_NODE = "add_node",
  REPLACE_NODE = "replace_node",
  ADD_CONNECTION = "add_connection",
  UPDATE_NODE_PROPERTY = "update_node_property",
}

export const AddNodePayloadSchema = z.object({
  graph_id: z.string(),
  name: z.string(),
  addon: z.string(),
  extension_group: z.string().optional(),
  app: z.string().optional(),
  property: z.record(z.string(), z.unknown()).optional(),
});

export const DeleteNodePayloadSchema = z.object({
  graph_id: z.string(),
  name: z.string(),
  addon: z.string(),
  extension_group: z.string().optional(),
  app: z.string().optional(),
});

export const AddConnectionPayloadSchema = z.object({
  graph_id: z.string(),
  src_app: z.string().nullable().optional(),
  src_extension: z.string(),
  msg_type: z.nativeEnum(EConnectionType),
  msg_name: z.string(),
  dest_app: z.string().nullable().optional(),
  dest_extension: z.string(),
  msg_conversion: z.unknown().optional(), // TODO: add msg_conversion type
});

export const DeleteConnectionPayloadSchema = z.object({
  graph_id: z.string(),
  src_app: z.string().nullable().optional(),
  src_extension: z.string(),
  msg_type: z.nativeEnum(EConnectionType),
  msg_name: z.string(),
  dest_app: z.string().nullable().optional(),
  dest_extension: z.string(),
});

export const UpdateNodePropertyPayloadSchema = z.object({
  graph_id: z.string(),
  name: z.string(),
  addon: z.string(),
  extension_group: z.string().optional(),
  app: z.string().optional(),
  property: stringToJSONSchema
    .pipe(z.record(z.string(), z.unknown()))
    .default({}),
});

export const GraphUiNodeGeometrySchema = z.object({
  app: z.string().optional(),
  id: z.string(),
  x: z.number(),
  y: z.number(),
});

export const SetGraphUiPayloadSchema = z.object({
  graph_id: z.string(),
  graph_geometry: z.object({
    nodes_geometry: z.array(GraphUiNodeGeometrySchema),
  }),
});

export enum EMsgDirection {
  IN = "in",
  OUT = "out",
}

export const MsgCompatiblePayloadSchema = z.object({
  graph_id: z.string(),
  app: z.string().optional(),
  extension_group: z.string().optional(),
  extension: z.string(),
  msg_type: z.nativeEnum(EConnectionType),
  msg_direction: z.nativeEnum(EMsgDirection),
  msg_name: z.string(),
});

export const MsgCompatibleResponseItemSchema = z.object({
  extension_group: z.string().optional(),
  extension: z.string(),
  msg_type: z.nativeEnum(EConnectionType),
  msg_direction: z.nativeEnum(EMsgDirection),
  msg_name: z.string().optional(),
});
