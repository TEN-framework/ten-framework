//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { z } from "zod";

import { stringToJSONSchema } from "@/utils";

export const DesignerFilterOperator = z.enum(["exact", "regex"]);
export type DesignerFilterOperator = z.infer<typeof DesignerFilterOperator>;

export const DesignerAtomicFilter = z.object({
  field: z.string(),
  operator: DesignerFilterOperator,
  value: z.string(),
});
export type DesignerAtomicFilter = z.infer<typeof DesignerAtomicFilter>;

export type DesignerFilter =
  | DesignerAtomicFilter
  | { and: DesignerFilter[] }
  | { or: DesignerFilter[] };
export const DesignerFilter: z.ZodType<DesignerFilter> = z.lazy(() =>
  z.union([
    DesignerAtomicFilter,
    z.object({
      and: z.array(z.lazy(() => DesignerFilter)),
    }),
    z.object({
      or: z.array(z.lazy(() => DesignerFilter)),
    }),
  ])
);

export const BackendNodeExtension = z.object({
  addon: z.string(),
  name: z.string(),
  app: z.string().nullish(),
  extension_group: z.string().nullish(),
  property: z.record(z.string(), z.unknown()).nullish(),
  api: z.unknown().nullish(),
  is_installed: z.boolean(),
  type: z.literal("extension"),
});
export type BackendNodeExtension = z.infer<typeof BackendNodeExtension>;

export const DesignerSelectorMessageInfo = z.object({
  msg_type: z.enum(["cmd", "data", "audio_frame", "video_frame"]),
  msg_name: z.string(),
  direction: z.enum(["in", "out"]),
  node_name: z.string(),
});
export type DesignerSelectorMessageInfo = z.infer<
  typeof DesignerSelectorMessageInfo
>;

export const BackendNodeSelector = z.object({
  name: z.string(),
  type: z.literal("selector"),
  filter: DesignerFilter,
  messages: z.array(DesignerSelectorMessageInfo).nullish(),
});
export type BackendNodeSelector = z.infer<typeof BackendNodeSelector>;

export const BackendNodeSubGraph = z.looseObject({
  type: z.literal("subgraph"),
}); // todo: selector&subgraph
export type BackendNodeSubGraph = z.infer<typeof BackendNodeSubGraph>;

export const BackendNode = z.discriminatedUnion("type", [
  BackendNodeExtension,
  BackendNodeSelector,
  BackendNodeSubGraph,
]);
export type BackendNode = z.infer<typeof BackendNode>;

export const GraphLoc = z.object({
  app: z.string().nullish(),
  extension: z.string().nullish(),
  subgraph: z.string().nullish(),
  selector: z.string().nullish(),
});
export type GraphLoc = z.infer<typeof GraphLoc>;

export const MsgConversionRule = z.object({
  path: z.string(),
  conversion_mode: z.enum(["fixed_value", "from_original"]),
  original_path: z.string().nullish(),
  value: z.record(z.string(), z.unknown()).nullish(),
});
export type MsgConversionRule = z.infer<typeof MsgConversionRule>;

export const MsgConversion = z.object({
  conversion_type: z.enum(["per_property"]),
  rules: z.array(MsgConversionRule),
});
export type MsgConversion = z.infer<typeof MsgConversion>;

export const MsgAndResultConversion = z.object({
  msg: MsgConversion.nullish(),
  result: MsgConversion.nullish(),
});
export type MsgAndResultConversion = z.infer<typeof MsgAndResultConversion>;

export const GraphDestination = GraphLoc.extend({
  msg_conversion: MsgAndResultConversion.nullish(),
});
export type GraphDestination = z.infer<typeof GraphDestination>;

export const GraphSource = GraphLoc.extend({});
export type GraphSource = z.infer<typeof GraphSource>;

export const GraphMessageFlow = z.object({
  name: z.string().nullish(),
  names: z.array(z.string()).nullish(),
  dest: z.array(GraphDestination).nullish(),
  source: z.array(GraphSource).nullish(),
});
export type GraphMessageFlow = z.infer<typeof GraphMessageFlow>;

export enum EConnectionType {
  CMD = "cmd",
  DATA = "data",
  AUDIO_FRAME = "audio_frame",
  VIDEO_FRAME = "video_frame",
}

export const GraphConnection = GraphLoc.extend({
  [EConnectionType.CMD]: z.array(GraphMessageFlow).nullish(),
  [EConnectionType.DATA]: z.array(GraphMessageFlow).nullish(),
  [EConnectionType.AUDIO_FRAME]: z.array(GraphMessageFlow).nullish(),
  [EConnectionType.VIDEO_FRAME]: z.array(GraphMessageFlow).nullish(),
});
export type GraphConnection = z.infer<typeof GraphConnection>;

export const GraphExposedMessageType = z.enum([
  "cmd_in",
  "cmd_out",
  "data_in",
  "data_out",
  "audio_frame_in",
  "audio_frame_out",
  "video_frame_in",
  "video_frame_out",
]);

export const GraphExposedMessage = z.object({
  type: GraphExposedMessageType,
  name: z.string(),
  extension: z.string().nullish(),
  subgraph: z.string().nullish(),
});
export type GraphExposedMessage = z.infer<typeof GraphExposedMessage>;

export const GraphExposedProperty = z.object({
  extension: z.string().nullish(),
  subgraph: z.string().nullish(),
  name: z.string(),
});
export type GraphExposedProperty = z.infer<typeof GraphExposedProperty>;

export const Graph = z.object({
  nodes: z.array(BackendNode),
  connections: z.array(GraphConnection),
  exposed_messages: z.array(GraphExposedMessage),
  exposed_properties: z.array(GraphExposedProperty),
});
export type Graph = z.infer<typeof Graph>;

export const GraphInfo = z.object({
  graph_id: z.string(),
  name: z.string().nullish(),
  auto_start: z.boolean().nullish(),
  base_dir: z.string().nullish(),
  graph: Graph,
});
export type GraphInfo = z.infer<typeof GraphInfo>;

/** @deprecated */
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
  extension_group: z.string().nullish(),
  app: z.string().nullish(),
  property: z.record(z.string(), z.unknown()).nullish(),
});

export const DeleteNodePayloadSchema = z.object({
  graph_id: z.string(),
  name: z.string(),
  addon: z.string(),
  extension_group: z.string().nullish(),
  app: z.string().nullish(),
});

export const AddConnectionPayloadSchema = z.object({
  graph_id: z.string(),
  src: GraphLoc,
  dest: GraphLoc,
  msg_type: z.enum(EConnectionType),
  msg_names: z.array(z.string()),
  msg_conversion: MsgAndResultConversion.nullish(),
});

export const DeleteConnectionPayloadSchema = z.object({
  graph_id: z.string(),
  src: GraphLoc,
  dest: GraphLoc,
  msg_type: z.enum(EConnectionType),
  msg_names: z.array(z.string()),
});

export const UpdateNodePropertyPayloadSchema = z.object({
  graph_id: z.string(),
  name: z.string(),
  addon: z.string(),
  extension_group: z.string().nullish(),
  app: z.string().nullish(),
  property: stringToJSONSchema
    .pipe(z.record(z.string(), z.unknown()))
    .default({}),
});

export const GraphUiNodeGeometrySchema = z.object({
  app: z.string().nullish(),
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
  app: z.string().nullish(),
  extension_group: z.string().nullish(),
  extension: z.string(),
  msg_type: z.nativeEnum(EConnectionType),
  msg_direction: z.nativeEnum(EMsgDirection),
  msg_name: z.string(),
});

export const MsgCompatibleResponseItemSchema = z.object({
  extension_group: z.string().nullish(),
  extension: z.string(),
  msg_type: z.nativeEnum(EConnectionType),
  msg_direction: z.nativeEnum(EMsgDirection),
  msg_name: z.string().nullish(),
});
