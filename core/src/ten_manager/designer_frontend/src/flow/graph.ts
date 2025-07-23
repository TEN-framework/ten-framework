//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { MarkerType } from "@xyflow/react";
import dagre from "dagre";

import { retrieveAddons } from "@/api/services/addons";
import {
  retrieveGraphConnections,
  retrieveGraphNodes,
} from "@/api/services/graphs";
import {
  postGetGraphNodeGeometry,
  postSetGraphNodeGeometry,
} from "@/api/services/storage";
import { data2identifier, EFlowElementIdentifier } from "@/lib/identifier";
import type { IExtensionAddon } from "@/types/apps";
import {
  ECustomNodeType,
  type TCustomEdge,
  type TCustomEdgeAddressMap,
  type TCustomNode,
  type TExtensionNode,
  type TGraphNode,
} from "@/types/flow";
import {
  EConnectionType,
  type IBackendConnection,
  type IBackendNode,
  type IGraph,
} from "@/types/graphs";

const NODE_WIDTH = 384;
const NODE_HEIGHT = 200;
const NODE_X_SPACING = 100;
const NODE_Y_SPACING = 100;

export const generateRawNodesFromSets = (
  sets: {
    backendNodes: IBackendNode[];
    graph: IGraph;
  }[]
): TCustomNode[] => {
  let currentX = 0;
  let currentY = 100;

  const results: TCustomNode[][] = [];

  for (const set of sets) {
    const { backendNodes, graph } = set;
    const {
      nodes: rawNodes,
      width: graphWidth,
      // height: graphHeight,
    } = generateRawNodes(backendNodes, graph, {
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
  backendNodes: IBackendNode[],
  graph: IGraph,
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
    id: graph.uuid,
    position: { x: startX, y: startY },
    type: "graphNode",
    data: {
      _type: ECustomNodeType.GRAPH,
      graph,
    },
    width: graphAreaWidth,
    height: graphAreaHeight,
  };

  const extensionNodes: TExtensionNode[] = backendNodes.map((n, idx) => {
    const row = Math.floor(idx / cols);
    const col = idx % cols;

    return {
      // id: `${graph.uuid}-${n.name}`,
      id: data2identifier(EFlowElementIdentifier.CUSTOM_NODE, {
        type: ECustomNodeType.EXTENSION,
        graph: graph.uuid,
        name: n.name,
      }),
      position: {
        x:
          NODE_X_SPACING + col * (NODE_WIDTH + NODE_X_SPACING) + NODE_X_SPACING,
        y:
          NODE_Y_SPACING +
          row * (NODE_HEIGHT + NODE_Y_SPACING) +
          NODE_Y_SPACING,
      },
      type: "extensionNode",
      parentId: graph.uuid,
      extent: "parent",
      data: {
        ...n,
        _type: ECustomNodeType.EXTENSION,
        graph: graph,
        src: {
          [EConnectionType.CMD]: [],
          [EConnectionType.DATA]: [],
          [EConnectionType.AUDIO_FRAME]: [],
          [EConnectionType.VIDEO_FRAME]: [],
        },
        target: {
          [EConnectionType.CMD]: [],
          [EConnectionType.DATA]: [],
          [EConnectionType.AUDIO_FRAME]: [],
          [EConnectionType.VIDEO_FRAME]: [],
        },
      },
    };
  });

  return {
    nodes: [graphNode, ...extensionNodes],
    width: graphAreaWidth,
    height: graphAreaHeight,
    startX,
    startY,
  };
};

export const generateRawEdges = (
  backendConnections: IBackendConnection[],
  graph: IGraph,
  options?: {
    // Optional default edges to include
    defaultEdges?: TCustomEdge[];
    // Optional default edge address map
    defaultEdgeAddressMap?: TCustomEdgeAddressMap;
  }
): [TCustomEdge[], TCustomEdgeAddressMap] => {
  const edgeAddressMap: TCustomEdgeAddressMap =
    options?.defaultEdgeAddressMap ?? {
      [EConnectionType.CMD]: [],
      [EConnectionType.DATA]: [],
      [EConnectionType.AUDIO_FRAME]: [],
      [EConnectionType.VIDEO_FRAME]: [],
    };
  const edges: TCustomEdge[] = options?.defaultEdges ?? [];

  backendConnections.forEach((connection) => {
    const extension = connection.extension;
    const app = connection.app;
    const TYPES = [
      EConnectionType.CMD,
      EConnectionType.DATA,
      EConnectionType.AUDIO_FRAME,
      EConnectionType.VIDEO_FRAME,
    ];
    TYPES.forEach((connectionType) => {
      if (!connection[connectionType]) {
        return;
      }
      connection[connectionType].forEach((connectionItem) => {
        const name = connectionItem.name;
        const dest = connectionItem.dest;
        dest.forEach((connectionItemDest) => {
          const targetExtension = connectionItemDest.extension;
          const targetApp = connectionItemDest.app;
          const edgeId = data2identifier(EFlowElementIdentifier.EDGE, {
            name,
            src: extension,
            target: targetExtension,
            graph: graph.uuid,
            connectionType,
          });
          // const edgeId = `edge-${extension}-${name}-${targetExtension}`;
          const edgeAddress = {
            extension: targetExtension,
            app: targetApp,
          };
          edgeAddressMap[connectionType].push({
            src: {
              extension,
              app,
            },
            target: edgeAddress,
            name,
            graph,
          });
          edges.push({
            id: edgeId,
            // source: extension,
            // target: targetExtension,
            source: data2identifier(EFlowElementIdentifier.CUSTOM_NODE, {
              type: ECustomNodeType.EXTENSION,
              graph: graph.uuid,
              name: extension,
            }),
            target: data2identifier(EFlowElementIdentifier.CUSTOM_NODE, {
              type: ECustomNodeType.EXTENSION,
              graph: graph.uuid,
              name: targetExtension,
            }),
            data: {
              graph,
              name,
              connectionType,
              extension,
              src: {
                extension,
                app,
              },
              target: edgeAddress,
              labelOffsetX: 0,
              labelOffsetY: 0,
            },
            type: "customEdge",
            label: name,
            sourceHandle: data2identifier(EFlowElementIdentifier.HANDLE, {
              type: "source",
              extension,
              graph: graph.uuid,
              connectionType: connectionType,
            }),
            targetHandle: data2identifier(EFlowElementIdentifier.HANDLE, {
              type: "target",
              extension: targetExtension,
              graph: graph.uuid,
              connectionType: connectionType,
            }),
            markerEnd: {
              type: MarkerType.ArrowClosed,
              width: 20,
              height: 20,
              color: "#FF0072",
            },
          });
        });
      });
    });
  });

  return [edges, edgeAddressMap];
};

export const updateNodesWithConnections = (
  nodes: TCustomNode[],
  edgeAddressMap: TCustomEdgeAddressMap
): TCustomNode[] => {
  // const extensionNodeNames: TExtensionNode[] = nodes.filter(
  //   (node) => node.data._type === ECustomNodeType.EXTENSION
  // );
  const results: TCustomNode[] = [];
  for (const node of nodes) {
    if (node.data._type === ECustomNodeType.EXTENSION) {
      [
        EConnectionType.CMD,
        EConnectionType.DATA,
        EConnectionType.AUDIO_FRAME,
        EConnectionType.VIDEO_FRAME,
      ].forEach((connectionType) => {
        const srcConnections = edgeAddressMap[connectionType].filter(
          (edge) =>
            edge.target.extension === node.data.name &&
            edge.graph.uuid === node.data.graph.uuid
        );
        const targetConnections = edgeAddressMap[connectionType].filter(
          (edge) =>
            edge.src.extension === node.data.name &&
            edge.graph.uuid === node.data.graph.uuid
        );
        (node as TExtensionNode).data.src[connectionType].push(
          ...srcConnections
        );
        (node as TExtensionNode).data.target[connectionType].push(
          ...targetConnections
        );
      });
    }
    results.push(node);
  }

  return results;
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
      acc[baseDir] = addonInfoMap;
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
    const targetAddonMap = cache[baseDir];
    if (!targetAddonMap.has(addonName)) {
      const addonInfoList: IExtensionAddon[] = await retrieveAddons({
        base_dir: baseDir,
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
    node.data.url = cache[baseDir].get(addonName)?.url;
  }

  return nodes;
};

/** @deprecated */
export const generateNodesAndEdges = (
  inputNodes: TCustomNode[],
  inputEdges: TCustomEdge[]
): { nodes: TCustomNode[]; edges: TCustomEdge[] } => {
  const dagreGraph = new dagre.graphlib.Graph();
  dagreGraph.setDefaultEdgeLabel(() => ({}));

  // Find all bidirectional pairs
  const nodePairs = new Map<string, Set<string>>();
  inputEdges.forEach((edge) => {
    const hasReverse = inputEdges.some(
      (e) => e.source === edge.target && e.target === edge.source
    );
    if (hasReverse) {
      if (!nodePairs.has(edge.source)) {
        nodePairs.set(edge.source, new Set());
      }
      nodePairs.get(edge.source)?.add(edge.target);
    }
  });

  // Set graph to flow top to bottom-right
  dagreGraph.setGraph({
    rankdir: "TB",
    nodesep: NODE_WIDTH * 2,
    ranksep: NODE_HEIGHT * 2,
  });

  // Add nodes to graph
  inputNodes.forEach((node) => {
    dagreGraph.setNode(node.id, { width: NODE_WIDTH, height: NODE_HEIGHT });
  });

  // Process pairs in order of first appearance
  const processedPairs = new Set<string>();
  let currentX = 0;
  const nodeXPositions = new Map<string, number>();

  inputEdges.forEach((edge) => {
    dagreGraph.setEdge(edge.source, edge.target);

    // Check if this forms a pair and hasn't been processed
    const pairKey = [edge.source, edge.target].sort().join("-");
    if (
      nodePairs.has(edge.source) &&
      nodePairs.get(edge.source)?.has(edge.target) &&
      !processedPairs.has(pairKey)
    ) {
      processedPairs.add(pairKey);
      nodeXPositions.set(edge.source, currentX);
      nodeXPositions.set(edge.target, currentX + NODE_WIDTH * 2);
      currentX += NODE_WIDTH * 4;
    }
  });

  dagre.layout(dagreGraph);

  const layoutedNodes = inputNodes.map((node) => {
    const nodeWithPosition = dagreGraph.node(node.id);
    const xPos = nodeXPositions.has(node.id)
      ? nodeXPositions.get(node.id)
      : nodeWithPosition.x;

    return {
      ...node,
      position: {
        x: (xPos ?? nodeWithPosition.x) - NODE_WIDTH / 2,
        y: nodeWithPosition.y - NODE_HEIGHT / 2,
      },
    };
  });

  const edgesWithNewHandles = inputEdges.map((edge) => {
    const type = edge.data?.connectionType;
    if (type) {
      return {
        ...edge,
        sourceHandle: `source-${edge.source}-${type}`,
        targetHandle: `target-${edge.target}-${type}`,
      };
    }
    return edge;
  });

  return { nodes: layoutedNodes, edges: edgesWithNewHandles };
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
      if (!acc[node.data.graph.uuid]) {
        acc[node.data.graph.uuid] = [];
      }
      acc[node.data.graph.uuid].push({
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
        (node) => node.data.graph.uuid === graphId
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

export const resetNodesAndEdgesByGraphs = async (graphs: IGraph[]) => {
  const backendNodes = await Promise.all(
    graphs.map(async (graph) => {
      const nodes = await retrieveGraphNodes(graph.uuid);
      return { graph: graph, nodes: nodes };
    })
  );
  const backendConnections = await Promise.all(
    graphs.map(async (graph) => {
      const connections = await retrieveGraphConnections(graph.uuid);
      return { graph: graph, connections: connections };
    })
  );
  const rawNodes = generateRawNodesFromSets(
    backendNodes.map((set) => ({
      backendNodes: set.nodes,
      graph: set.graph,
    }))
  );
  let rawEdges: TCustomEdge[] = [];
  let rawEdgeAddressMap: TCustomEdgeAddressMap = {
    [EConnectionType.CMD]: [],
    [EConnectionType.DATA]: [],
    [EConnectionType.AUDIO_FRAME]: [],
    [EConnectionType.VIDEO_FRAME]: [],
  };
  backendConnections.forEach((set) => {
    const [edges, edgeAddressMap] = generateRawEdges(
      set.connections,
      set.graph,
      {
        defaultEdges: rawEdges,
        defaultEdgeAddressMap: rawEdgeAddressMap,
      }
    );
    rawEdges = edges;
    rawEdgeAddressMap = edgeAddressMap;
  });
  const nodesWithConnections = updateNodesWithConnections(
    rawNodes,
    rawEdgeAddressMap
  );
  const nodesWithAddonInfo =
    await updateNodesWithAddonInfo(nodesWithConnections);
  const nodesWithGeometry = await syncGraphNodeGeometry(nodesWithAddonInfo);
  return { nodes: nodesWithGeometry, edges: rawEdges };
};
