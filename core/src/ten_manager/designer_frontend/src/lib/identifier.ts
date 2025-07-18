//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import type { ECustomNodeType } from "@/types/flow";
import type { EConnectionType } from "@/types/graphs";

export enum EFlowElementIdentifier {
  EDGE = "edge",
  HANDLE = "handle",
  COMMON_NODE = "common-node",
}
export type TEdgeData = {
  name: string;
  src: string;
  target: string;
  graph: string;
  connectionType: EConnectionType;
};

export const data2EdgeId = (
  //   identifier: EFlowElementIdentifier.EDGE,
  data: TEdgeData
): string => {
  const sortedKeys: Array<keyof TEdgeData> = [
    "name",
    "src",
    "target",
    "graph",
    "connectionType",
  ];
  return (
    `identifier:${EFlowElementIdentifier.EDGE};` +
    sortedKeys.map((keyName) => `${keyName}:${data[keyName]}`).join(";")
  );
};

export type THandleData = {
  type: "source" | "target";
  extension: string;
  graph: string;
  connectionType: EConnectionType;
};

export const data2HandleId = (
  //   identifier: EFlowElementIdentifier.HANDLE,
  data: THandleData
): string => {
  const sortedKeys: Array<keyof THandleData> = [
    "type",
    "extension",
    "graph",
    "connectionType",
  ];
  return (
    `identifier:${EFlowElementIdentifier.HANDLE};` +
    sortedKeys.map((keyName) => `${keyName}:${data[keyName]}`).join(";")
  );
};

export type TCommonNodeData = {
  type: ECustomNodeType;
  graph: string;
  name: string;
};

export const data2ExtensionNodeId = (data: TCommonNodeData): string => {
  const sortedKeys: Array<keyof TCommonNodeData> = ["type", "graph", "name"];
  return (
    `identifier:${EFlowElementIdentifier.COMMON_NODE};` +
    sortedKeys.map((keyName) => `${keyName}:${data[keyName]}`).join(";")
  );
};

export const data2identifier = (
  identifier: EFlowElementIdentifier,
  data: TEdgeData | THandleData | TCommonNodeData
) => {
  if (identifier === EFlowElementIdentifier.EDGE) {
    return data2EdgeId(data as TEdgeData);
  } else if (identifier === EFlowElementIdentifier.HANDLE) {
    return data2HandleId(data as THandleData);
  } else if (identifier === EFlowElementIdentifier.COMMON_NODE) {
    return data2ExtensionNodeId(data as TCommonNodeData);
  }
  throw new Error(`Unknown identifier type: ${identifier}`);
};
