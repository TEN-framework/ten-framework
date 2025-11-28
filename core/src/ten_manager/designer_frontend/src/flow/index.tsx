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
import EdgeContextMenu from "@/flow/context-menu/edge-context-menu";
import { CustomEdge } from "@/flow/edge";
import { syncGraphNodeGeometry } from "@/flow/graph";
import { ExtensionNode, SelectorNode, SubgraphNode } from "@/flow/node";
import { GraphNode } from "@/flow/node/graph";
import { cn } from "@/lib/utils";
import { useFlowStore } from "@/store";
import {
  ECustomNodeType,
  type TCustomEdge,
  type TCustomNode,
} from "@/types/flow";

import "@xyflow/react/dist/style.css"; // Import react-flow style.
import "@/flow/reactflow.css"; // Import react-flow style.

const nodeTypes = {
  [ECustomNodeType.EXTENSION]: ExtensionNode,
  [ECustomNodeType.GRAPH]: GraphNode,
  [ECustomNodeType.SELECTOR]: SelectorNode,
  [ECustomNodeType.SUB_GRAPH]: SubgraphNode,
};

export const FlowCanvas = (props: { className?: string }) => {
  const { className } = props;

  const [contextMenu, setContextMenu] = React.useState<{
    visible: boolean;
    x: number;
    y: number;
    type?: "node" | "edge" | "pane";
    edge?: TCustomEdge;
  }>({ visible: false, x: 0, y: 0 });

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
    (changes: NodeChange<TCustomNode>[]) => {
      const newNodes = applyNodeChanges(changes, displayedNodes);
      const positionChanges = changes.filter(
        (change) => change.type === "position" && change.dragging === false
      );
      if (positionChanges?.length > 0) {
        syncGraphNodeGeometry(newNodes, {
          forceLocal: true,
        });
      }
      setDisplayedNodes(newNodes);
    },
    [displayedNodes, setDisplayedNodes]
  );

  const onEdgesChange = React.useCallback(
    (changes: EdgeChange<TCustomEdge>[]) => {
      const newEdges = applyEdgeChanges(changes, displayedEdges);
      setDisplayedEdges(newEdges);
    },
    [displayedEdges, setDisplayedEdges]
  );

  /** @deprecated */
  const renderContextMenu = () => {
    if (contextMenu.type === "edge" && contextMenu.edge) {
      return (
        <EdgeContextMenu
          visible={contextMenu.visible}
          x={contextMenu.x}
          y={contextMenu.y}
          edge={contextMenu.edge}
          onClose={() => {
            setContextMenu({ visible: false, x: 0, y: 0 });
          }}
        />
      );
    }
    return null;
  };

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
        onEdgeContextMenu={(event, edge) => {
          event.preventDefault();
          setContextMenu({
            visible: true,
            x: event.clientX,
            y: event.clientY,
            type: "edge",
            edge: {
              id: edge.id,
              source: edge.source,
              target: edge.target,
              type: edge.type,
              data: edge.data as TCustomEdge["data"],
            },
          });
        }}
        onBlur={() => {
          if (contextMenu.visible) {
            setTimeout(() => {
              setContextMenu({ visible: false, x: 0, y: 0 });
            }, 300);
          }
        }}
        fitView
        nodesDraggable
        edgesFocusable
        deleteKeyCode={null}
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
      {renderContextMenu()}
    </div>
  );
};
