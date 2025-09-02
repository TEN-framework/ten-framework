//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import { type NodeProps, Position } from "@xyflow/react";
import {
  AudioLinesIcon,
  DatabaseIcon,
  TerminalIcon,
  VideoIcon,
} from "lucide-react";
import * as React from "react";
import { Button } from "@/components/ui/button";
import {
  ContextMenu,
  ContextMenuContent,
  ContextMenuTrigger,
} from "@/components/ui/context-menu";
import { BaseHandle } from "@/components/ui/react-flow/base-handle";
import { BaseNode } from "@/components/ui/react-flow/base-node";
import { Separator } from "@/components/ui/separator";
import { NODE_CONFIG_MAPPING } from "@/flow/node/base";
import { data2identifier, EFlowElementIdentifier } from "@/lib/identifier";
import { cn } from "@/lib/utils";
import { useFlowStore } from "@/store";
import type { ISelectorNodeData, TSelectorNode } from "@/types/flow";
import { EConnectionType } from "@/types/graphs";

const CONFIG = NODE_CONFIG_MAPPING.selector;

export function SelectorNode(props: NodeProps<TSelectorNode>) {
  const { data } = props;

  return (
    <ContextMenu>
      <ContextMenuTrigger asChild>
        <BaseNode
          className={cn(
            "w-fit min-w-3xs p-0 shadow-md",
            "rounded-md border bg-popover"
          )}
        >
          {/* Header section */}
          <div
            className={cn(
              "rounded-t-md rounded-b-md px-4 py-2",
              "flex items-center gap-2",
              "w-full"
            )}
          >
            {/* Node type icon */}
            <CONFIG.Icon className={cn("size-4")} />
            {/* Content */}
            <div className="flex flex-1 flex-col truncate">
              <span className={cn("font-medium text-foreground text-sm")}>
                {data.name}
              </span>
              <span className={cn("text-muted-foreground/50 text-xs")}>
                {data.type}
              </span>
            </div>
          </div>
          {/* Connection handles section */}
          <Separator />
          <div className={cn("py-1")}>
            <div className="space-y-1">
              <HandleGroupItem
                data={data}
                connectionType={EConnectionType.CMD}
              />
              <Separator className={cn("my-1")} />
              <HandleGroupItem
                data={data}
                connectionType={EConnectionType.DATA}
              />
              <Separator className={cn("my-1")} />
              <HandleGroupItem
                data={data}
                connectionType={EConnectionType.AUDIO_FRAME}
              />
              <Separator className={cn("my-1")} />
              <HandleGroupItem
                data={data}
                connectionType={EConnectionType.VIDEO_FRAME}
              />
            </div>
          </div>
        </BaseNode>
      </ContextMenuTrigger>
      <ContextMenuContent className="w-fit">
        ContextMenuContent
      </ContextMenuContent>
    </ContextMenu>
  );
}

const HandleGroupItem = (props: {
  connectionType: EConnectionType;
  data: ISelectorNodeData;
}) => {
  const { data, connectionType } = props;

  const { edges } = useFlowStore();

  const connectionsInfo: {
    input: Record<EConnectionType, number>;
    output: Record<EConnectionType, number>;
  } = React.useMemo(() => {
    const initData: {
      input: Record<EConnectionType, number>;
      output: Record<EConnectionType, number>;
    } = {
      input: {
        data: 0,
        cmd: 0,
        audio_frame: 0,
        video_frame: 0,
      },
      output: {
        data: 0,
        cmd: 0,
        audio_frame: 0,
        video_frame: 0,
      },
    };
    const filteredEdges = edges.filter(
      (edge) => edge.data?.graph?.graph_id === data.graph.graph_id
    );

    const outputEdges = filteredEdges.filter(
      (edge) =>
        edge.data?.source?.name === data.name &&
        edge.data?.source?.type === data._type &&
        edge.data?.connectionType === connectionType
    );
    const inputEdges = filteredEdges.filter(
      (edge) =>
        edge.data?.target?.name === data.name &&
        edge.data?.target?.type === data._type &&
        edge.data?.connectionType === connectionType
    );

    initData.input[connectionType] = inputEdges.length;
    initData.output[connectionType] = outputEdges.length;

    return initData;
  }, [edges, data, connectionType]);

  return (
    <div
      className={cn(
        "relative",
        "flex items-center justify-between gap-x-4 px-1"
      )}
    >
      <div className="flex items-center gap-x-2">
        <BaseHandle
          key={`target-${data.name}-${connectionType}`}
          type="target"
          position={Position.Left}
          id={data2identifier(EFlowElementIdentifier.HANDLE, {
            type: "target",
            graph: data.graph.graph_id,
            connectionType,
            nodeName: data.name,
            nodeType: data._type,
          })}
          isConnectable={false}
          className={cn("size-3")}
        />
        <Button
          size="sm"
          variant="ghost"
          className={cn(
            "w-8 rounded-md px-1 py-0.5 text-center text-xs",
            "cursor-pointer"
          )}
        >
          <span>{connectionsInfo.input[connectionType]}</span>
        </Button>
      </div>

      <Button
        size="sm"
        variant="ghost"
        className={cn(
          "flex items-center gap-x-2 px-3 py-1.5",
          "font-medium text-xs",
          "cursor-pointer"
        )}
        onClick={() => {}}
      >
        {connectionType === EConnectionType.CMD && (
          <TerminalIcon className="size-3 shrink-0 text-blue-600" />
        )}
        {connectionType === EConnectionType.DATA && (
          <DatabaseIcon className="size-3 shrink-0 text-green-600" />
        )}
        {connectionType === EConnectionType.AUDIO_FRAME && (
          <AudioLinesIcon className="size-3 shrink-0 text-purple-600" />
        )}
        {connectionType === EConnectionType.VIDEO_FRAME && (
          <VideoIcon className="size-3 shrink-0 text-red-600" />
        )}
        <span className="text-gray-700 dark:text-green-200">
          {connectionType.toUpperCase()}
        </span>
      </Button>

      <div className="flex items-center gap-x-2">
        <Button
          size="sm"
          variant="ghost"
          className={cn(
            "w-8 rounded-md px-1 py-0.5 text-center text-xs",
            "cursor-pointer"
          )}
        >
          <span>{connectionsInfo.output[connectionType]}</span>
        </Button>
        <BaseHandle
          key={`source-${data.name}-${connectionType}`}
          type="source"
          position={Position.Right}
          id={data2identifier(EFlowElementIdentifier.HANDLE, {
            type: "source",
            graph: data.graph.graph_id,
            connectionType,
            nodeName: data.name,
            nodeType: data._type,
          })}
          isConnectable={false}
          className={cn("size-3")}
        />
      </div>
    </div>
  );
};
