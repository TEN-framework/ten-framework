//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as React from "react";
import {
  applyEdgeChanges,
  applyNodeChanges,
  EdgeChange,
  NodeChange,
} from "@xyflow/react";
import { toast } from "sonner";
import { QueryClientProvider } from "@tanstack/react-query";
import { ReactQueryDevtools } from "@tanstack/react-query-devtools";

import { ThemeProvider } from "@/components/ThemeProvider";
import AppBar from "@/components/AppBar";
import StatusBar from "@/components/StatusBar";
import FlowCanvas, { type FlowCanvasRef } from "@/flow/FlowCanvas";
import { generateNodesAndEdges, syncGraphNodeGeometry } from "@/flow/graph";
import {
  ResizableHandle,
  ResizablePanel,
  ResizablePanelGroup,
} from "@/components/ui/Resizable";
import { GlobalDialogs } from "@/components/GlobalDialogs";
// import Dock from "@/components/Dock";
import { useWidgetStore, useFlowStore, useAppStore } from "@/store";
import { EWidgetDisplayType } from "@/types/widgets";
import { GlobalPopups } from "@/components/Popup";
import { BackstageWidgets } from "@/components/Widget/BackstageWidgets";
import { cn } from "@/lib/utils";
import {
  usePreferencesLogViewerLines,
  initPersistentStorageSchema,
  getStorageValueByKey,
  setStorageValueByKey,
} from "@/api/services/storage";
import { PERSISTENT_DEFAULTS } from "@/constants/persistent";
import { SpinnerLoading } from "@/components/Status/Loading";
import { PREFERENCES_SCHEMA_LOG } from "@/types/apps";
import { getTanstackQueryClient } from "@/api/services/utils";

import type { TCustomEdge, TCustomNode } from "@/types/flow";

const queryClient = getTanstackQueryClient();

export default function App() {
  return (
    <>
      <ThemeProvider defaultTheme="system" storageKey="vite-ui-theme">
        <QueryClientProvider client={queryClient}>
          <ReactQueryDevtools initialIsOpen={false} />
          <Main />
        </QueryClientProvider>
      </ThemeProvider>
    </>
  );
}

const Main = () => {
  const { nodes, setNodes, edges, setEdges, setNodesAndEdges } = useFlowStore();
  const [resizablePanelMode] = React.useState<"left" | "bottom" | "right">(
    "bottom"
  );
  const [isPersistentSchemaInited, setIsPersistentSchemaInitialized] =
    React.useState(false);

  const {
    data: remotePreferencesLogViewerLines,
    isLoading: isLoadingPreferences,
    error: errorPreferences,
  } = usePreferencesLogViewerLines();

  const { widgets } = useWidgetStore();
  const { setPreferences, currentWorkspace } = useAppStore();

  const flowCanvasRef = React.useRef<FlowCanvasRef | null>(null);

  const dockWidgetsMemo = React.useMemo(
    () =>
      widgets.filter(
        (widget) => widget.display_type === EWidgetDisplayType.Dock
      ),
    [widgets]
  );

  const performAutoLayout = React.useCallback(() => {
    const { nodes: layoutedNodes, edges: layoutedEdges } =
      generateNodesAndEdges(nodes, edges);
    setNodesAndEdges(layoutedNodes, layoutedEdges);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [nodes, edges]);

  const handleNodesChange = React.useCallback(
    (changes: NodeChange<TCustomNode>[]) => {
      const newNodes = applyNodeChanges(changes, nodes);
      const positionChanges = changes.filter(
        (change) => change.type === "position" && change.dragging === false
      );
      if (positionChanges?.length > 0 && currentWorkspace?.graph?.uuid) {
        syncGraphNodeGeometry(currentWorkspace!.graph!.uuid, newNodes, {
          forceLocal: true,
        });
      }
      setNodes(newNodes);
    },
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [nodes]
  );

  const handleEdgesChange = React.useCallback(
    (changes: EdgeChange<TCustomEdge>[]) => {
      const newEdges = applyEdgeChanges(changes, edges);
      setEdges(newEdges);
    },
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [edges]
  );

  // init preferences
  React.useEffect(() => {
    if (
      remotePreferencesLogViewerLines &&
      remotePreferencesLogViewerLines?.logviewer_line_size
    ) {
      const parsedValues = PREFERENCES_SCHEMA_LOG.safeParse(
        remotePreferencesLogViewerLines
      );
      if (!parsedValues.success) {
        throw new Error("Invalid values");
      }
      setPreferences(
        "logviewer_line_size",
        parsedValues.data.logviewer_line_size
      );
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [remotePreferencesLogViewerLines]);

  React.useEffect(() => {
    if (errorPreferences) {
      console.error(errorPreferences);
      toast.error(errorPreferences.message);
    }
  }, [errorPreferences]);

  // Initialize the persistent storage schema if it is not initialized.
  React.useEffect(() => {
    const init = async () => {
      if (!isPersistentSchemaInited) {
        try {
          const remoteCfg = await getStorageValueByKey();
          if (remoteCfg?.version === PERSISTENT_DEFAULTS.version) {
            // If the schema is already initialized,
            // we can skip the initialization.
            return;
          }
          // If the schema is not initialized, we need to initialize it.
          await initPersistentStorageSchema();
          // After initialization, we can set the default values.
          await setStorageValueByKey();
        } catch (error) {
          console.error("Failed to initialize persistent schema:", error);
          toast.error("Failed to initialize persistent schema.");
        } finally {
          // Do not block the UI if any error occurs during initialization.
          setIsPersistentSchemaInitialized(true);
        }
      }
    };

    init();
  }, [isPersistentSchemaInited]);

  if (isLoadingPreferences || !isPersistentSchemaInited) {
    return (
      <div className="flex items-center justify-center h-screen w-full">
        <SpinnerLoading />
      </div>
    );
  }

  return (
    <>
      <AppBar onAutoLayout={performAutoLayout} className="z-9997" />

      <ResizablePanelGroup
        key={`resizable-panel-group-${resizablePanelMode}`}
        direction={resizablePanelMode === "bottom" ? "vertical" : "horizontal"}
        className={cn("w-screen h-screen", "min-h-screen min-w-screen")}
      >
        {resizablePanelMode === "left" && dockWidgetsMemo.length > 0 && (
          <>
            <ResizablePanel defaultSize={40}>
              {/* Global dock widgets. */}
              {/* <Dock
                position={resizablePanelMode}
                onPositionChange={
                  setResizablePanelMode as (position: string) => void
                }
              /> */}
            </ResizablePanel>
            <ResizableHandle />
          </>
        )}
        <ResizablePanel defaultSize={dockWidgetsMemo.length > 0 ? 60 : 100}>
          <FlowCanvas
            ref={flowCanvasRef}
            nodes={nodes}
            edges={edges}
            onNodesChange={handleNodesChange}
            onEdgesChange={handleEdgesChange}
            // onConnect={(connection) => {
            //   const newEdges = addEdge(connection, edges);
            //   setEdges(newEdges);
            // }}
            onConnect={() => {}}
            className="w-full h-[calc(100dvh-60px)] mt-10"
          />
        </ResizablePanel>
        {resizablePanelMode !== "left" && dockWidgetsMemo.length > 0 && (
          <>
            <ResizableHandle />
            <ResizablePanel defaultSize={40}>
              {/* Global dock widgets. */}
              {/* <Dock
                position={resizablePanelMode}
                onPositionChange={
                  setResizablePanelMode as (position: string) => void
                }
              /> */}
            </ResizablePanel>
          </>
        )}
      </ResizablePanelGroup>

      {/* Global popups. */}
      <GlobalPopups />

      {/* Global dialogs. */}
      <GlobalDialogs />

      {/* [invisible] Global backstage widgets. */}
      <BackstageWidgets />

      <StatusBar className="z-9997" />
    </>
  );
};
