//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import {
  applyEdgeChanges,
  applyNodeChanges,
  Background,
  BackgroundVariant,
  Controls,
  type EdgeChange,
  MiniMap,
  type NodeChange,
  ReactFlow,
} from "@xyflow/react";
import * as React from "react";
import { ThemeProviderContext } from "@/components/theme-context";
import CustomEdge from "@/flow/CustomEdge";
import { syncGraphNodeGeometry } from "@/flow/graph";
import { ExtensionNode } from "@/flow/node";
import { GraphNode } from "@/flow/node/graph";
import { cn } from "@/lib/utils";
import { useAppStore, useFlowStore } from "@/store";
import type { TCustomEdge, TExtensionNode } from "@/types/flow";

import "@xyflow/react/dist/style.css"; // Import react-flow style.
import "@/flow/reactflow.css"; // Import react-flow style.

const nodeTypes = {
  extensionNode: ExtensionNode,
  graphNode: GraphNode,
};

export const FlowCanvas = (props: { className?: string }) => {
  const { className } = props;

  const { currentWorkspace } = useAppStore(); // todo: remove

  const {
    displayedNodes,
    setDisplayedNodes,
    displayedEdges,
    setDisplayedEdges,
  } = useFlowStore();

  const { theme } = React.useContext(ThemeProviderContext);

  //   const onConnect: OnConnect = React.useCallback(
  //     (params) => {
  //       setDisplayedEdges((edges) =>
  //         addEdge({ type: "data", data: { key: "value" }, ...params }, edges)
  //       );
  //     },
  //     [setDisplayedEdges]
  //   );

  const onNodesChange = React.useCallback(
    (changes: NodeChange<TExtensionNode>[]) => {
      const newNodes = applyNodeChanges(changes, displayedNodes);
      const positionChanges = changes.filter(
        (change) => change.type === "position" && change.dragging === false
      );
      if (positionChanges?.length > 0 && currentWorkspace?.graph?.uuid) {
        syncGraphNodeGeometry(currentWorkspace?.graph?.uuid, newNodes, {
          forceLocal: true,
        });
      }
      setDisplayedNodes(newNodes);
    },
    [currentWorkspace?.graph?.uuid, displayedNodes, setDisplayedNodes]
  );

  const onEdgesChange = React.useCallback(
    (changes: EdgeChange<TCustomEdge>[]) => {
      const newEdges = applyEdgeChanges(changes, displayedEdges);
      setDisplayedEdges(newEdges);
    },
    [displayedEdges, setDisplayedEdges]
  );

  return (
    <div
      className={cn("flow-container h-[calc(100vh-40px)] w-full", className)}
    >
      <ReactFlow
        colorMode={theme}
        nodes={displayedNodes}
        edges={displayedEdges}
        onNodesChange={onNodesChange}
        onEdgesChange={onEdgesChange}
        // onConnect={onConnect}
        nodeTypes={nodeTypes}
        edgeTypes={{
          customEdge: CustomEdge,
        }}
        fitView
        nodesDraggable
        edgesFocusable
        style={{ width: "100%", height: "100%" }}
      >
        <Controls />
        <MiniMap zoomable pannable />
        <Background
          variant={BackgroundVariant.Dots}
          gap={12}
          size={1}
          color={theme === "dark" ? "#333" : "#ccc"}
        />
      </ReactFlow>
    </div>
  );
};
