//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

/** biome-ignore-all lint/suspicious/noExplicitAny: <ignore> */

import * as z from "zod";
import { ECustomNodeType } from "@/types/flow";
import { EConnectionType } from "@/types/graphs";

export enum EFlowElementIdentifier {
  EDGE = "edge",
  HANDLE = "handle",
  CUSTOM_NODE = "custom-node",
}

export const IdentifierEdgeData = z.object({
  name: z.string(),
  names: z
    .array(z.string())
    .nullish()
    .transform((i) => {
      return i?.sort()?.join(",") || i;
    }),
  sourceNode: z.string(),
  sourceNodeType: z.enum(ECustomNodeType),
  targetNode: z.string(),
  targetNodeType: z.enum(ECustomNodeType),
  graph: z.string(),
  connectionType: z.enum(EConnectionType),
  isReversed: z.boolean(),
});
export type IdentifierEdgeData = z.infer<typeof IdentifierEdgeData>;

export const data2EdgeId = (data: IdentifierEdgeData): string => {
  const sortedKeys = Object.keys(IdentifierEdgeData.shape).sort();
  return (
    `identifier:${EFlowElementIdentifier.EDGE};` +
    sortedKeys
      .map(
        (keyName) => `${keyName}:${data[keyName as keyof IdentifierEdgeData]}`
      )
      .join(";")
  );
};

export const IdentifierHandleData = z.object({
  type: z.enum(["source", "target"]),
  nodeName: z.string(),
  nodeType: z.enum(ECustomNodeType),
  graph: z.string(),
  connectionType: z.enum(EConnectionType),
});
export type IdentifierHandleData = z.infer<typeof IdentifierHandleData>;

export const data2HandleId = (data: IdentifierHandleData): string => {
  const sortedKeys = Object.keys(IdentifierHandleData.shape).sort();
  return (
    `identifier:${EFlowElementIdentifier.HANDLE};` +
    sortedKeys
      .map(
        (keyName) => `${keyName}:${data[keyName as keyof IdentifierHandleData]}`
      )
      .join(";")
  );
};

export const IdentifierCustomNodeData = z.object({
  type: z.enum(ECustomNodeType),
  graph: z.string(),
  name: z.string(),
});
export type IdentifierCustomNodeData = z.infer<typeof IdentifierCustomNodeData>;

export const data2ExtensionNodeId = (
  data: IdentifierCustomNodeData
): string => {
  const sortedKeys = Object.keys(IdentifierCustomNodeData.shape).sort();
  return (
    `identifier:${EFlowElementIdentifier.CUSTOM_NODE};` +
    sortedKeys
      .map(
        (keyName) =>
          `${keyName}:${data[keyName as keyof IdentifierCustomNodeData]}`
      )
      .join(";")
  );
};

export const data2identifier = (
  identifier: EFlowElementIdentifier,
  data: IdentifierEdgeData | IdentifierHandleData | IdentifierCustomNodeData
) => {
  if (identifier === EFlowElementIdentifier.EDGE) {
    return data2EdgeId(data as IdentifierEdgeData);
  } else if (identifier === EFlowElementIdentifier.HANDLE) {
    return data2HandleId(data as IdentifierHandleData);
  } else if (identifier === EFlowElementIdentifier.CUSTOM_NODE) {
    return data2ExtensionNodeId(data as IdentifierCustomNodeData);
  }
  throw new Error(`Unknown identifier type: ${identifier}`);
};

export const identifier2data = <
  T extends
    | IdentifierEdgeData
    | IdentifierHandleData
    | IdentifierCustomNodeData,
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
    }, {} as IdentifierEdgeData) as T;
  } else if (type === EFlowElementIdentifier.HANDLE) {
    return parts.reduce((acc, part) => {
      const [key, value] = part.split(":");
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      if (key !== "identifier") (acc as any)[key] = value;
      return acc;
    }, {} as IdentifierHandleData) as T;
  } else if (type === EFlowElementIdentifier.CUSTOM_NODE) {
    return parts.reduce((acc, part) => {
      const [key, value] = part.split(":");
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      if (key !== "identifier") (acc as any)[key] = value;
      return acc;
    }, {} as IdentifierCustomNodeData) as T;
  }
  throw new Error(`Unknown identifier type: ${type}`);
};
