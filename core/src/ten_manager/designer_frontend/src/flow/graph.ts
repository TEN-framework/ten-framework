//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { MarkerType } from "@xyflow/react";
import dagre from "dagre";

import { retrieveAddons } from "@/api/services/addons";
import type { IExtensionAddon } from "@/types/apps";
import type {
  TCustomNode,
  TCustomEdge,
  TCustomEdgeAddressMap,
} from "@/types/flow";
import {
  type IBackendNode,
  type IBackendConnection,
  EConnectionType,
} from "@/types/graphs";
import {
  postGetGraphNodeGeometry,
  postSetGraphNodeGeometry,
} from "@/api/services/graphs";

const NODE_WIDTH = 172;
const NODE_HEIGHT = 48;

export const generateRawNodes = (
  backendNodes: IBackendNode[]
): TCustomNode[] => {
  return backendNodes.map((n, idx) => ({
    id: n.name,
    position: { x: idx * 200, y: 100 },
    type: "customNode",
    data: {
      name: n.name,
      addon: n.addon,
      extension_group: n.extension_group,
      app: n.app,
      property: n.property,
      api: n.api,
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
  }));
};

export const generateRawEdges = (
  backendConnections: IBackendConnection[]
): [TCustomEdge[], TCustomEdgeAddressMap] => {
  const edgeAddressMap: TCustomEdgeAddressMap = {
    [EConnectionType.CMD]: [],
    [EConnectionType.DATA]: [],
    [EConnectionType.AUDIO_FRAME]: [],
    [EConnectionType.VIDEO_FRAME]: [],
  };
  const edges: TCustomEdge[] = [];

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
          const edgeId = `edge-${extension}-${name}-${targetExtension}`;
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
          });
          edges.push({
            id: edgeId,
            source: extension,
            target: targetExtension,
            data: {
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
            sourceHandle: `source-${extension}`,
            targetHandle: `target-${targetExtension}`,
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
  nodes.forEach((node) => {
    [
      EConnectionType.CMD,
      EConnectionType.DATA,
      EConnectionType.AUDIO_FRAME,
      EConnectionType.VIDEO_FRAME,
    ].forEach((connectionType) => {
      const srcConnections = edgeAddressMap[connectionType].filter(
        (edge) => edge.target.extension === node.data.name
      );
      const targetConnections = edgeAddressMap[connectionType].filter(
        (edge) => edge.src.extension === node.data.name
      );
      node.data.src[connectionType].push(...srcConnections);
      node.data.target[connectionType].push(...targetConnections);
    });
  });
  return nodes;
};

export const updateNodesWithAddonInfo = async (
  baseDir: string,
  nodes: TCustomNode[]
): Promise<TCustomNode[]> => {
  const nodesAddonNameList = [...new Set(nodes.map((node) => node.data.addon))];
  const addonInfoMap = new Map<string, IExtensionAddon>();
  for (const addonName of nodesAddonNameList) {
    const addonInfoList: IExtensionAddon[] = await retrieveAddons({
      base_dir: baseDir,
      addon_name: addonName,
    });
    const addonInfo: IExtensionAddon | undefined = addonInfoList?.[0];
    if (!addonInfo) {
      console.warn(`Addon '${addonName}' not found`);
      continue;
    }
    addonInfoMap.set(addonName, addonInfo);
  }
  return nodes.map((node) => {
    const addonInfo = addonInfoMap.get(node.data.addon);
    if (!addonInfo) {
      console.warn(
        `Node '${node.data.name}' has addon '${node.data.addon}' not found`
      );
      return node;
    }
    node.data.url = addonInfo.url;
    return node;
  });
};

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
  graphId: string,
  nodes: TCustomNode[],
  options: {
    forceLocal?: boolean; // override all nodes geometry
  } = {}
): Promise<TCustomNode[]> => {
  const isForceLocal = options.forceLocal ?? false;

  const localNodesGeometry = nodes.map((node) => ({
    extension: node.data.name,
    x: parseInt(String(node.position.x), 10),
    y: parseInt(String(node.position.y), 10),
  }));

  // If forceLocal is true, set geometry to backend and return
  if (isForceLocal) {
    try {
      await postSetGraphNodeGeometry({
        graph_id: graphId,
        graph_geometry: {
          nodes_geometry: localNodesGeometry,
        },
      });
    } catch (error) {
      console.error("Error syncing graph node geometry", error);
    }
    return nodes;
  }

  // If force is false, merge local geometry with remote geometry
  try {
    // Retrieve geometry from backend
    const remoteNodesGeometry = await postGetGraphNodeGeometry(graphId);

    // const mergedNodesGeometry = localNodesGeometry.reduce((prev, node) => {
    //   const remoteNode = prev.find((g) => g.extension === node.extension);
    //   if (remoteNode) {
    //     remoteNode.x = parseInt(String(node.x), 10);
    //     remoteNode.y = parseInt(String(node.y), 10);
    //     return prev;
    //   }
    //   return [...prev, node];
    // }, remoteNodesGeometry);

    const mergedNodesGeometry = localNodesGeometry.map((node) => {
      const remoteNode = remoteNodesGeometry.find(
        (g) => g.extension === node.extension
      );
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

    // Update nodes with geometry
    const nodesWithGeometry = nodes.map((node) => {
      const geometry = mergedNodesGeometry.find(
        (g) => g.extension === node.data.name
      );
      if (geometry) {
        return {
          ...node,
          position: {
            x: parseInt(String(geometry.x), 10),
            y: parseInt(String(geometry.y), 10),
          },
        };
      }
    });

    return nodesWithGeometry as TCustomNode[];
  } catch (error) {
    console.error("Error syncing graph node geometry", error);
    return nodes;
  }
};
