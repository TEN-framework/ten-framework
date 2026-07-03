//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import type { Edge, Node } from "@xyflow/react";
import type {
  BackendNodeExtension,
  BackendNodeSelector,
  BackendNodeSubGraph,
  EConnectionType,
  GraphInfo,
} from "@/types/graphs";

export enum ECustomNodeType {
  GRAPH = "graph",
  EXTENSION = "extension",
  SELECTOR = "selector",
  SUB_GRAPH = "subgraph",
}

export interface IExtensionNodeData extends BackendNodeExtension {
  _type: ECustomNodeType.EXTENSION;
  graph: GraphInfo;
  url?: string; // ? need to be removed(ws)
  [key: string]: unknown;
}
export type TExtensionNode = Node<
  IExtensionNodeData,
  ECustomNodeType.EXTENSION
>;

export interface IGraphNodeData {
  _type: ECustomNodeType.GRAPH;
  graph: GraphInfo;
  [key: string]: unknown;
}
export type TGraphNode = Node<IGraphNodeData, ECustomNodeType.GRAPH>;

export interface ISelectorNodeData extends BackendNodeSelector {
  _type: ECustomNodeType.SELECTOR;
  graph: GraphInfo;
  [key: string]: unknown;
}
export type TSelectorNode = Node<ISelectorNodeData, ECustomNodeType.SELECTOR>;

// todo: refine it
export interface ISubGraphNodeData extends BackendNodeSubGraph {
  _type: ECustomNodeType.SUB_GRAPH;
  graph: GraphInfo;
  [key: string]: unknown;
}
export type TSubGraphNode = Node<ISubGraphNodeData, ECustomNodeType.SUB_GRAPH>;

export type TCustomNode =
  | TGraphNode
  | TExtensionNode
  | TSelectorNode
  | TSubGraphNode;

export type TCustomEdgeData = {
  labelOffsetX: number;
  labelOffsetY: number;
  connectionType: EConnectionType;
  app?: string;
  name: string;
  names?: string[];
  graph: GraphInfo;
  source: {
    type: ECustomNodeType;
    name: string;
    app?: string | null;
  };
  target: {
    type: ECustomNodeType;
    name: string;
    app?: string | null;
  };
  isReversed: boolean;
};

export type TCustomEdge = Edge<TCustomEdgeData, "customEdge">;
