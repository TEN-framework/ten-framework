//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import type { Edge, Node } from "@xyflow/react";
import type { EConnectionType, IBackendNode, IGraph } from "@/types/graphs";

export enum ECustomNodeType {
  GRAPH = "graph",
  EXTENSION = "extension",
  SELECTOR = "selector",
  SUB_GRAPH = "sub-graph",
}

export interface IExtensionNodeData extends IBackendNode {
  _type: ECustomNodeType.EXTENSION;
  graph: IGraph;
  src: Record<EConnectionType, TCustomEdgeAddressData[]>;
  target: Record<EConnectionType, TCustomEdgeAddressData[]>;
  url?: string; // ? need to be removed(ws)
  [key: string]: unknown;
}
export type TExtensionNode = Node<IExtensionNodeData, "extensionNode">;

export interface IGraphNodeData {
  _type: ECustomNodeType.GRAPH;
  graph: IGraph;
  [key: string]: unknown;
}
export type TGraphNode = Node<IGraphNodeData, "graphNode">;

export type TCustomNode = TGraphNode | TExtensionNode;

export type TCustomEdgeAddress = {
  extension: string;
  app?: string;
};

export type TCustomEdgeData = {
  labelOffsetX: number;
  labelOffsetY: number;
  connectionType: EConnectionType;
  app?: string;
  extension: string;
  src: TCustomEdgeAddress;
  target: TCustomEdgeAddress;
  name: string;
  graph: IGraph;
};

export type TCustomEdgeAddressData = {
  src: TCustomEdgeAddress;
  target: TCustomEdgeAddress;
  name: string;
  graph: IGraph;
};

export type TCustomEdgeAddressMap = Record<
  EConnectionType,
  TCustomEdgeAddressData[]
>;

export type TCustomEdge = Edge<TCustomEdgeData, "customEdge">;
