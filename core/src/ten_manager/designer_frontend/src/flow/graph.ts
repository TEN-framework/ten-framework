//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { MarkerType } from "@xyflow/react";
import { retrieveAddons } from "@/api/services/addons";
import {
  postGetGraphNodeGeometry,
  postSetGraphNodeGeometry,
} from "@/api/services/storage";
import { data2identifier, EFlowElementIdentifier } from "@/lib/identifier";
import type { IExtensionAddon } from "@/types/apps";
import {
  ECustomNodeType,
  type TCustomEdge,
  type TCustomNode,
  type TExtensionNode,
  type TGraphNode,
  type TSelectorNode,
} from "@/types/flow";
import {
  type BackendNode,
  EConnectionType,
  type GraphConnection,
  type GraphDestination,
  type GraphInfo,
  type GraphMessageFlow,
  type GraphSource,
} from "@/types/graphs";

const NODE_WIDTH = 384;
const NODE_HEIGHT = 200;
const NODE_X_SPACING = 100;
const NODE_Y_SPACING = 100;

export const generateRawNodesFromGraphs = (
  graphs: GraphInfo[]
): TCustomNode[] => {
  let currentX = 0;
  let currentY = 100;

  const results: TCustomNode[][] = [];

  for (const graph of graphs) {
    const {
      nodes: rawNodes,
      width: graphWidth,
      // height: graphHeight,
    } = generateRawNodes(graph.graph?.nodes || [], graph, {
      startX: currentX,
      startY: currentY,
    });
    currentX += graphWidth + NODE_X_SPACING;
    currentY += 0;

    results.push(rawNodes);
  }

  return results.flat();
};

export const generateRawNodes = (
  backendNodes: BackendNode[],
  graph: GraphInfo,
  options?: {
    startX?: number; // Optional starting X position for the graph node
    startY?: number; // Optional starting Y position for the graph node
  }
): {
  nodes: TCustomNode[];
  width: number;
  height: number;
  startX: number;
  startY: number;
} => {
  const startX = options?.startX ?? 0;
  const startY = options?.startY ?? 0;
  const extensionNodesLength = backendNodes.length;

  // Calculate grid dimensions - try to make it roughly square
  const cols = Math.ceil(Math.sqrt(extensionNodesLength));
  const rows = Math.ceil(extensionNodesLength / cols);

  // Calculate graph area dimensions with buffer space (2 nodes worth)
  const bufferWidth = NODE_X_SPACING * 2;
  const bufferHeight = NODE_Y_SPACING * 2;
  const graphAreaWidth =
    cols * NODE_WIDTH + NODE_X_SPACING * (cols + 1) + bufferWidth;
  const graphAreaHeight =
    rows * NODE_HEIGHT + NODE_Y_SPACING * (rows + 1) + bufferHeight;

  const graphNode: TGraphNode = {
    id: graph.graph_id,
    position: { x: startX, y: startY },
    type: ECustomNodeType.GRAPH,
    data: {
      _type: ECustomNodeType.GRAPH,
      graph,
    },
    width: graphAreaWidth,
    height: graphAreaHeight,
  };

  const parsedBackendNodes: (TExtensionNode | TSelectorNode | undefined)[] =
    backendNodes.map((n, idx) => {
      const row = Math.floor(idx / cols);
      const col = idx % cols;

      // extension node
      if (n.type === "extension") {
        return {
          id: data2identifier(EFlowElementIdentifier.CUSTOM_NODE, {
            type: ECustomNodeType.EXTENSION,
            graph: graph.graph_id,
            name: n.name,
          }),
          position: {
            x:
              NODE_X_SPACING +
              col * (NODE_WIDTH + NODE_X_SPACING) +
              NODE_X_SPACING,
            y:
              NODE_Y_SPACING +
              row * (NODE_HEIGHT + NODE_Y_SPACING) +
              NODE_Y_SPACING,
          },
          type: ECustomNodeType.EXTENSION,
          parentId: graph.graph_id,
          extent: "parent",
          data: {
            ...n,
            _type: ECustomNodeType.EXTENSION,
            graph: graph,
          },
        } as TExtensionNode;
      }

      // selector node
      if (n.type === "selector") {
        return {
          id: data2identifier(EFlowElementIdentifier.CUSTOM_NODE, {
            type: ECustomNodeType.SELECTOR,
            graph: graph.graph_id,
            name: n.name,
          }),
          position: {
            x:
              NODE_X_SPACING +
              col * (NODE_WIDTH + NODE_X_SPACING) +
              NODE_X_SPACING,
            y:
              NODE_Y_SPACING +
              row * (NODE_HEIGHT + NODE_Y_SPACING) +
              NODE_Y_SPACING,
          },
          type: ECustomNodeType.SELECTOR,
          parentId: graph.graph_id,
          extent: "parent",
          data: {
            ...n,
            _type: ECustomNodeType.SELECTOR,
            graph: graph,
          },
        };
      }

      // subgraph node
      // todo
      return undefined;
    });

  const customNodes: TCustomNode[] = parsedBackendNodes.filter(
    Boolean
  ) as TCustomNode[];

  return {
    nodes: [graphNode, ...customNodes],
    width: graphAreaWidth,
    height: graphAreaHeight,
    startX,
    startY,
  };
};

export const inferNodeTypeFromConnection = (
  connection: GraphConnection | GraphDestination | GraphSource
): { type: ECustomNodeType; name: string; app?: string } => {
  if (connection.selector) {
    return {
      type: ECustomNodeType.SELECTOR,
      name: connection.selector,
      app: connection.app || undefined,
    };
  }
  if (connection.subgraph) {
    return {
      type: ECustomNodeType.SUB_GRAPH,
      name: connection.subgraph,
      app: connection.app || undefined,
    };
  }
  if (connection.extension) {
    return {
      type: ECustomNodeType.EXTENSION,
      name: connection.extension,
      app: connection.app || undefined,
    };
  }
  throw new Error("Unknown connection type");
};

export const generateRawEdgesFromGraph = (
  graphInfo: GraphInfo
): TCustomEdge[] => {
  const edges: TCustomEdge[] = [];
  const connections = graphInfo.graph?.connections || [];
  for (const connection of connections) {
    for (const connectionType of Object.values(EConnectionType)) {
      const typedConnections =
        (connection?.[
          connectionType as keyof typeof connection
        ] as GraphMessageFlow[]) || [];

      for (const typedConnection of typedConnections) {
        // source only or destination only
        const { name, names, source, dest } = typedConnection;
        const typedConnectionBaseName = name
          ? name
          : [...(names || [])].sort().join(",");
        // handle `dest` first
        // if `dest` exists, `connection` -> `dest`
        if (dest && dest.length > 0) {
          for (const d of dest) {
            const targetNode = inferNodeTypeFromConnection(
              d as GraphDestination
            );
            const sourceNode = inferNodeTypeFromConnection(
              connection as GraphSource
            );
            const edgeId = data2identifier(EFlowElementIdentifier.EDGE, {
              name: typedConnectionBaseName,
              names,
              sourceNode: sourceNode.name,
              sourceNodeType: sourceNode.type,
              targetNode: targetNode.name,
              targetNodeType: targetNode.type,
              graph: graphInfo.graph_id,
              connectionType: connectionType as EConnectionType,
              isReversed: false,
            });
            edges.push({
              id: edgeId,
              source: data2identifier(EFlowElementIdentifier.CUSTOM_NODE, {
                type: sourceNode.type,
                graph: graphInfo.graph_id,
                name: sourceNode.name,
              }),
              target: data2identifier(EFlowElementIdentifier.CUSTOM_NODE, {
                type: targetNode.type,
                graph: graphInfo.graph_id,
                name: targetNode.name,
              }),
              data: {
                graph: graphInfo,
                name: typedConnectionBaseName,
                names: names || undefined,
                connectionType: connectionType as EConnectionType,
                source: {
                  type: sourceNode.type,
                  app: sourceNode.app,
                  name: sourceNode.name,
                },
                target: {
                  type: targetNode.type,
                  app: targetNode.app,
                  name: targetNode.name,
                },
                labelOffsetX: 0,
                labelOffsetY: 0,
                isReversed: false,
              },
              type: "customEdge",
              label: name,
              sourceHandle: data2identifier(EFlowElementIdentifier.HANDLE, {
                type: "source",
                graph: graphInfo.graph_id,
                connectionType: connectionType as EConnectionType,
                nodeName: sourceNode.name,
                nodeType: sourceNode.type,
              }),
              targetHandle: data2identifier(EFlowElementIdentifier.HANDLE, {
                type: "target",
                graph: graphInfo.graph_id,
                connectionType: connectionType as EConnectionType,
                nodeName: targetNode.name,
                nodeType: targetNode.type,
              }),
              markerEnd: {
                type: MarkerType.ArrowClosed,
                width: 20,
                height: 20,
                color: "#FF0072",
              },
            });
          }
        }
        // otherwise handle `source`
        // `source` -> `connection`
        else if (source && source.length > 0) {
          for (const s of source) {
            const targetNode = inferNodeTypeFromConnection(
              connection as GraphConnection
            );
            const sourceNode = inferNodeTypeFromConnection(s as GraphSource);
            const edgeId = data2identifier(EFlowElementIdentifier.EDGE, {
              name: typedConnectionBaseName,
              names,
              sourceNode: sourceNode.name,
              sourceNodeType: sourceNode.type,
              targetNode: targetNode.name,
              targetNodeType: targetNode.type,
              graph: graphInfo.graph_id,
              connectionType: connectionType as EConnectionType,
              isReversed: true,
            });
            edges.push({
              id: edgeId,
              source: data2identifier(EFlowElementIdentifier.CUSTOM_NODE, {
                type: sourceNode.type,
                graph: graphInfo.graph_id,
                name: sourceNode.name,
              }),
              target: data2identifier(EFlowElementIdentifier.CUSTOM_NODE, {
                type: targetNode.type,
                graph: graphInfo.graph_id,
                name: targetNode.name,
              }),
              data: {
                graph: graphInfo,
                name: typedConnectionBaseName,
                names: names || undefined,
                connectionType: connectionType as EConnectionType,
                source: {
                  type: sourceNode.type,
                  app: sourceNode.app,
                  name: sourceNode.name,
                },
                target: {
                  type: targetNode.type,
                  app: targetNode.app,
                  name: targetNode.name,
                },
                labelOffsetX: 0,
                labelOffsetY: 0,
                isReversed: true,
              },
              type: "customEdge",
              label: name,
              sourceHandle: data2identifier(EFlowElementIdentifier.HANDLE, {
                type: "source",
                graph: graphInfo.graph_id,
                connectionType: connectionType as EConnectionType,
                nodeName: sourceNode.name,
                nodeType: sourceNode.type,
              }),
              targetHandle: data2identifier(EFlowElementIdentifier.HANDLE, {
                type: "target",
                graph: graphInfo.graph_id,
                connectionType: connectionType as EConnectionType,
                nodeName: targetNode.name,
                nodeType: targetNode.type,
              }),
              markerEnd: {
                type: MarkerType.ArrowClosed,
                width: 20,
                height: 20,
                color: "#FF0072",
              },
            });
          }
        }
      }
    }
  }

  return edges;
};

export const updateNodesWithAddonInfo = async (
  nodes: TCustomNode[]
): Promise<TCustomNode[]> => {
  const graphsBaseDirList = [
    ...new Set(nodes.map((node) => node.data.graph.base_dir)),
  ];
  const cache = graphsBaseDirList.reduce(
    (acc, baseDir) => {
      const addonInfoMap = new Map<string, IExtensionAddon>();
      acc[baseDir ?? ""] = addonInfoMap;
      return acc;
    },
    {} as Record<string, Map<string, IExtensionAddon>>
  );

  for (const node of nodes) {
    if (node.data._type !== ECustomNodeType.EXTENSION) {
      continue;
    }
    const baseDir = node.data.graph.base_dir;
    const addonName = node.data.addon;
    const targetAddonMap = cache[baseDir ?? ""];
    if (!targetAddonMap.has(addonName)) {
      const addonInfoList: IExtensionAddon[] = await retrieveAddons({
        base_dir: baseDir ?? "",
        addon_name: addonName,
      });
      const addonInfo: IExtensionAddon | undefined = addonInfoList?.[0];
      if (!addonInfo) {
        console.warn(`Addon '${addonName}' not found`);
        targetAddonMap.set(addonName, addonInfo);
        continue;
      }
      targetAddonMap.set(addonName, addonInfo);
    }
    node.data.url = cache[baseDir ?? ""].get(addonName)?.url;
  }

  return nodes;
};

export const syncGraphNodeGeometry = async (
  nodes: TCustomNode[],
  options: {
    forceLocal?: boolean; // override all nodes geometry
  } = {}
): Promise<TCustomNode[]> => {
  const isForceLocal = options.forceLocal ?? false;

  const localNodesGeometryMappings = nodes.reduce(
    (acc, node) => {
      if (!acc[node.data.graph.graph_id]) {
        acc[node.data.graph.graph_id] = [];
      }
      acc[node.data.graph.graph_id].push({
        id: node.id,
        x: node.position.x,
        y: node.position.y,
      });
      return acc;
    },
    {} as Record<string, { id: string; x: number; y: number }[]>
  );

  // If forceLocal is true, set geometry to backend and return
  if (isForceLocal) {
    try {
      for (const graphId in localNodesGeometryMappings) {
        const nodeGeometry = localNodesGeometryMappings[graphId];
        await postSetGraphNodeGeometry({
          graph_id: graphId,
          graph_geometry: {
            nodes_geometry: nodeGeometry,
          },
        });
      }
    } catch (error) {
      console.error("Error syncing graph node geometry", error);
    }
    return nodes;
  }

  // If force is false, merge local geometry with remote geometry
  const updatedNodes: TCustomNode[] = [];

  try {
    for (const graphId in localNodesGeometryMappings) {
      const localNodesGeometry = localNodesGeometryMappings[graphId];

      // Retrieve geometry from backend
      const remoteNodesGeometry = await postGetGraphNodeGeometry(graphId);

      const mergedNodesGeometry = localNodesGeometry.map((node) => {
        const remoteNode = remoteNodesGeometry.find((g) => g.id === node.id);
        if (remoteNode) {
          return {
            ...node,
            x: parseInt(String(remoteNode.x), 10),
            y: parseInt(String(remoteNode.y), 10),
          };
        }
        return node;
      });

      await postSetGraphNodeGeometry({
        graph_id: graphId,
        graph_geometry: {
          nodes_geometry: mergedNodesGeometry,
        },
      });

      // Update nodes with geometry for this graph
      const graphNodes = nodes.filter(
        (node) => node.data.graph.graph_id === graphId
      );
      const nodesWithGeometry = graphNodes.map((node) => {
        const geometry = mergedNodesGeometry.find((g) => g.id === node.id);
        if (geometry) {
          return {
            ...node,
            position: {
              x: parseInt(String(geometry.x), 10),
              y: parseInt(String(geometry.y), 10),
            },
          };
        }
        return node;
      });

      updatedNodes.push(...nodesWithGeometry);
    }

    return updatedNodes;
  } catch (error) {
    console.error("Error syncing graph node geometry", error);
    return nodes;
  }
};

export const resetNodesAndEdgesByGraphs = async (graphs: GraphInfo[]) => {
  const rawNodes = generateRawNodesFromGraphs(graphs);
  const rawEdges: TCustomEdge[] = [];

  for (const graph of graphs) {
    const rawEdgesSlice = generateRawEdgesFromGraph(graph);
    rawEdges.push(...rawEdgesSlice);
  }

  const nodesWithAddonInfo = await updateNodesWithAddonInfo(rawNodes);
  const nodesWithGeometry = await syncGraphNodeGeometry(nodesWithAddonInfo);
  return { nodes: nodesWithGeometry, edges: rawEdges };
};
