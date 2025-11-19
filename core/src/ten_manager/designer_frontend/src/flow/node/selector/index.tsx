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
// eslint-disable-next-line max-len
import { CustomNodeConnPopupTitle } from "@/components/popup/custom-node-connection";
import { Button } from "@/components/ui/button";
import {
  ContextMenu,
  ContextMenuContent,
  ContextMenuTrigger,
} from "@/components/ui/context-menu";
import { BaseHandle } from "@/components/ui/react-flow/base-handle";
import { BaseNode } from "@/components/ui/react-flow/base-node";
import { Separator } from "@/components/ui/separator";
import {
  CONTAINER_DEFAULT_ID,
  GROUP_CUSTOM_CONNECTION_ID,
} from "@/constants/widgets";
import { CustomNodeConnectionButton } from "@/flow/edge/button";
import { NODE_CONFIG_MAPPING } from "@/flow/node/base";
import { ContextMenuItems } from "@/flow/node/selector/context-menu";
import { data2identifier, EFlowElementIdentifier } from "@/lib/identifier";
import { cn } from "@/lib/utils";
import { useFlowStore, useWidgetStore } from "@/store";
import type {
  ECustomNodeType,
  ISelectorNodeData,
  TSelectorNode,
} from "@/types/flow";
import { EConnectionType, type GraphInfo } from "@/types/graphs";
import { EWidgetCategory, EWidgetDisplayType } from "@/types/widgets";

const CONFIG = NODE_CONFIG_MAPPING.selector;

export function SelectorNode(props: NodeProps<TSelectorNode>) {
  const { data } = props;

  return (
    <ContextMenu>
      <ContextMenuTrigger asChild>
        <BaseNode
          className={cn(
            "w-fit min-w-3xs p-0 shadow-md",
            "rounded-md border border-dashed",
            "bg-slate-100/90 dark:bg-slate-900/90"
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
        <ContextMenuItems node={props as unknown as TSelectorNode} />
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
  const { appendWidget } = useWidgetStore();

  const handleLaunchConnPopup = (data: {
    source: {
      name: string;
      type: ECustomNodeType;
    };
    target?: {
      name: string;
      type: ECustomNodeType;
    };
    graph: GraphInfo;
    metadata?: {
      filters?: {
        type?: EConnectionType;
        source?: boolean;
        target?: boolean;
      };
    };
  }) => {
    const { source, target, metadata, graph } = data;
    const id = `${source}-${target ?? ""}`;
    const filters = metadata?.filters;
    appendWidget({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: GROUP_CUSTOM_CONNECTION_ID,
      widget_id: id,

      category: EWidgetCategory.CustomConnection,
      display_type: EWidgetDisplayType.Popup,

      title: (
        <CustomNodeConnPopupTitle source={source.name} target={target?.name} />
      ),
      metadata: { id, source, target, filters, graph },

      popup: {
        height: 0.8,
        width: 0.6,
        maxHeight: 0.8,
        maxWidth: 0.6,
      },
    });
  };

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
        <CustomNodeConnectionButton
          data={{
            source: {
              name: data.name,
              type: data._type,
            },
            target: undefined,
            graph: data.graph,
            metadata: {
              filters: {
                type: connectionType,
                target: true,
              },
            },
          }}
          size="sm"
          variant="ghost"
          className={cn(
            "w-8 rounded-md px-1 py-0.5 text-center text-xs",
            "cursor-pointer"
          )}
        >
          <span>{connectionsInfo.input[connectionType]}</span>
        </CustomNodeConnectionButton>
      </div>

      <Button
        size="sm"
        variant="ghost"
        className={cn(
          "flex items-center gap-x-2 px-3 py-1.5",
          "font-medium text-xs",
          "cursor-pointer"
        )}
        onClick={() => {
          handleLaunchConnPopup({
            source: {
              name: data.name,
              type: data._type,
            },
            target: undefined,
            graph: data.graph,
            metadata: {
              filters: {
                type: connectionType,
                source: true,
              },
            },
          });
        }}
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
        <CustomNodeConnectionButton
          data={{
            source: {
              name: data.name,
              type: data._type,
            },
            target: undefined,
            graph: data.graph,
          }}
          size="sm"
          variant="ghost"
          className={cn(
            "w-8 rounded-md px-1 py-0.5 text-center text-xs",
            "cursor-pointer"
          )}
        >
          <span>{connectionsInfo.output[connectionType]}</span>
        </CustomNodeConnectionButton>
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
