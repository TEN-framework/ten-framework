//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { z } from "zod";

import { stringToJSONSchema } from "@/utils";

export interface IBackendNode {
  addon: string;
  name: string;
  extension_group?: string;
  app?: string;
  property?: Record<string, unknown>;
  api?: unknown;
  is_installed: boolean;
}

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
    dest: {
      app?: string;
      extension: string;
    }[];
  }[];
  [EConnectionType.DATA]?: {
    name: string;
    dest: {
      app?: string;
      extension: string;
    }[];
  }[];
  [EConnectionType.AUDIO_FRAME]?: {
    name: string;
    dest: {
      app?: string;
      extension: string;
    }[];
  }[];
  [EConnectionType.VIDEO_FRAME]?: {
    name: string;
    dest: {
      app?: string;
      extension: string;
    }[];
  }[];
}

export interface IGraph {
  name: string;
  auto_start: boolean;
  base_dir: string;
  uuid: string;
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
    .default("{}"),
});

export const GraphUiNodeGeometrySchema = z.object({
  app: z.string().optional(),
  extension: z.string(),
  x: z.number(),
  y: z.number(),
});

export const SetGraphUiPayloadSchema = z.object({
  graph_id: z.string(),
  graph_geometry: z.object({
    nodes_geometry: z.array(GraphUiNodeGeometrySchema),
  }),
});
