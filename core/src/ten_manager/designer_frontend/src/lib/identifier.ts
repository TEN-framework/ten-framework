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
  CUSTOM_NODE = "custom-node",
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

export type TCustomNodeData = {
  type: ECustomNodeType;
  graph: string;
  name: string;
};

export const data2ExtensionNodeId = (data: TCustomNodeData): string => {
  const sortedKeys: Array<keyof TCustomNodeData> = ["type", "graph", "name"];
  return (
    `identifier:${EFlowElementIdentifier.CUSTOM_NODE};` +
    sortedKeys.map((keyName) => `${keyName}:${data[keyName]}`).join(";")
  );
};

export const data2identifier = (
  identifier: EFlowElementIdentifier,
  data: TEdgeData | THandleData | TCustomNodeData
) => {
  if (identifier === EFlowElementIdentifier.EDGE) {
    return data2EdgeId(data as TEdgeData);
  } else if (identifier === EFlowElementIdentifier.HANDLE) {
    return data2HandleId(data as THandleData);
  } else if (identifier === EFlowElementIdentifier.CUSTOM_NODE) {
    return data2ExtensionNodeId(data as TCustomNodeData);
  }
  throw new Error(`Unknown identifier type: ${identifier}`);
};

export const identifier2data = <
  T extends TEdgeData | THandleData | TCustomNodeData,
>(
  identifier: string
): T => {
  const parts = identifier.split(";");
  const typePart = parts.find((p) => p.startsWith("identifier:"));
  if (!typePart) throw new Error(`Invalid identifier: ${identifier}`);

  const type = typePart.split(":")[1];
  if (type === EFlowElementIdentifier.EDGE) {
    return parts.reduce((acc, part) => {
      const [key, value] = part.split(":");
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      if (key !== "identifier") (acc as any)[key] = value;
      return acc;
    }, {} as TEdgeData) as T;
  } else if (type === EFlowElementIdentifier.HANDLE) {
    return parts.reduce((acc, part) => {
      const [key, value] = part.split(":");
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      if (key !== "identifier") (acc as any)[key] = value;
      return acc;
    }, {} as THandleData) as T;
  } else if (type === EFlowElementIdentifier.CUSTOM_NODE) {
    return parts.reduce((acc, part) => {
      const [key, value] = part.split(":");
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      if (key !== "identifier") (acc as any)[key] = value;
      return acc;
    }, {} as TCustomNodeData) as T;
  }
  throw new Error(`Unknown identifier type: ${type}`);
};
