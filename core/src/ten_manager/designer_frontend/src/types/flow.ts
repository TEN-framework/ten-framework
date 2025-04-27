//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import type { Node, Edge } from "@xyflow/react";

import type { IBackendNode, EConnectionType } from "@/types/graphs";

export type TCustomNodeData = Partial<IBackendNode> & {
  addon: string;
  name: string;
  extension_group?: string;
  app?: string;
  url?: string;
  src: Record<EConnectionType, TCustomEdgeAddressData[]>;
  target: Record<EConnectionType, TCustomEdgeAddressData[]>;
};

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
