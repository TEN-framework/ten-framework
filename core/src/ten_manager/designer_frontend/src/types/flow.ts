//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import type { Edge, Node } from "@xyflow/react";
import type { EConnectionType, IBackendNode } from "@/types/graphs";

export enum ECustomNodeType {
  EXTENSION = "extension",
  SELECTOR = "selector",
  SUB_GRAPH = "sub-graph",
}

export interface IExtensionNodeData extends IBackendNode {
  _type: ECustomNodeType.EXTENSION;
  src: Record<EConnectionType, TCustomEdgeAddressData[]>;
  target: Record<EConnectionType, TCustomEdgeAddressData[]>;
  url?: string; // ? need to be removed(ws)
  [key: string]: unknown;
}
export type TExtensionNode = Node<IExtensionNodeData, "extensionNode">;

export type TCommonNode = TExtensionNode;

/** @deprecated */
export type TCustomNodeData = Partial<IBackendNode> & {
  addon: string;
  name: string;
  extension_group?: string;
  app?: string;
  url?: string;
  src: Record<EConnectionType, TCustomEdgeAddressData[]>;
  target: Record<EConnectionType, TCustomEdgeAddressData[]>;
};

/** @deprecated */
export type TCustomNode = Node<TCustomNodeData, "customNode">;

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
};

export type TCustomEdgeAddressData = {
  src: TCustomEdgeAddress;
  target: TCustomEdgeAddress;
  name: string;
};

export type TCustomEdgeAddressMap = Record<
  EConnectionType,
  TCustomEdgeAddressData[]
>;

export type TCustomEdge = Edge<TCustomEdgeData, "customEdge">;
