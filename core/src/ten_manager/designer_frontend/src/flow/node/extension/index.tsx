//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import {
  type Connection,
  type Edge,
  type NodeProps,
  Position,
} from "@xyflow/react";
import {
  AudioLinesIcon,
  ChevronDownIcon,
  DatabaseIcon,
  PuzzleIcon,
  TerminalIcon,
  VideoIcon,
} from "lucide-react";
import React from "react";
import { useTranslation } from "react-i18next";
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
  Tooltip,
  TooltipContent,
  TooltipProvider,
  TooltipTrigger,
} from "@/components/ui/tooltip";
import {
  CONTAINER_DEFAULT_ID,
  GROUP_CUSTOM_CONNECTION_ID,
} from "@/constants/widgets";
import { CustomNodeConnectionButton } from "@/flow/edge/button";
import { ContextMenuItems } from "@/flow/node/extension/context-menu";
import { data2identifier, EFlowElementIdentifier } from "@/lib/identifier";
import { cn } from "@/lib/utils";
import { useWidgetStore } from "@/store";
import type { IExtensionNodeData, TExtensionNode } from "@/types/flow";
import { EConnectionType, type IGraph } from "@/types/graphs";
import { EWidgetCategory, EWidgetDisplayType } from "@/types/widgets";

export function ExtensionNode(props: NodeProps<TExtensionNode>) {
  const { data, isConnectable } = props;

  const [isDetailed, setIsDetailed] = React.useState(true);

  if (!isDetailed) {
    // Thumbnail mode
    return (
      <BaseNode className={cn("w-xs p-0 shadow-md", "border bg-popover")}>
        <ExtensionNodeHeader
          data={data}
          onExpandedClick={() => {
            setIsDetailed(true);
          }}
        >
          {/* Target handle for thumbnail mode */}
          <BaseHandle
            type="target"
            position={Position.Top}
            id={`target-${data.name}-thumbnail`}
            isConnectable={false}
            className="h-3 w-3"
          />

          {/* Source handle for thumbnail mode */}
          <BaseHandle
            type="source"
            position={Position.Bottom}
            id={`source-${data.name}-thumbnail`}
            isConnectable={false}
            className="size-3"
          />
        </ExtensionNodeHeader>
      </BaseNode>
    );
  }

  // Detailed mode
  return (
    <ContextMenu>
      <ContextMenuTrigger asChild>
        <BaseNode className={cn("w-xs p-0 shadow-md", "border bg-popover")}>
          {/* Header section */}
          <ExtensionNodeHeader
            data={data}
            isExpanded={isDetailed}
            onExpandedClick={() => {
              setIsDetailed(false);
            }}
            className="rounded-b-none"
          />
          {/* Connection handles section */}
          <Separator />
          <div className={cn("py-1")}>
            <div className="space-y-1">
              <HandleGroupItem
                data={data}
                isConnectable={isConnectable}
                connectionType={EConnectionType.CMD}
              />
              <Separator className={cn("my-1")} />
              <HandleGroupItem
                data={data}
                isConnectable={isConnectable}
                connectionType={EConnectionType.DATA}
              />
              <Separator className={cn("my-1")} />
              <HandleGroupItem
                data={data}
                isConnectable={isConnectable}
                connectionType={EConnectionType.AUDIO_FRAME}
              />
              <Separator className={cn("my-1")} />
              <HandleGroupItem
                data={data}
                isConnectable={isConnectable}
                connectionType={EConnectionType.VIDEO_FRAME}
              />
            </div>
          </div>
        </BaseNode>
      </ContextMenuTrigger>
      <ContextMenuContent className="w-fit">
        <ContextMenuItems
          node={props as unknown as TExtensionNode}
          baseDir={data.graph.base_dir}
          graphId={data.graph.uuid}
        />
      </ContextMenuContent>
    </ContextMenu>
  );
}

const ExtensionNodeHeader = (props: {
  className?: string;
  data: IExtensionNodeData;
  isDetailed?: boolean;
  isExpanded?: boolean;
  onExpandedClick: () => void;
  children?: React.ReactNode;
}) => {
  const { data, isExpanded, onExpandedClick, className, children } = props;

  const [isInstalled] = React.useMemo(() => {
    return [data.is_installed];
  }, [data]);

  const { t } = useTranslation();

  return (
    <div
      className={cn(
        "rounded-t-md rounded-b-md px-4 py-2",
        "flex items-center gap-2",
        className
      )}
    >
      {/* Connection indicator */}
      <TooltipProvider delayDuration={50}>
        <Tooltip>
          <TooltipTrigger asChild>
            <div
              className={cn(
                "size-2 cursor-help rounded-full",
                isInstalled ? "bg-green-500" : "bg-gray-400"
              )}
            />
          </TooltipTrigger>
          <TooltipContent>
            <p>{isInstalled ? t("node.installed") : t("node.uninstalled")}</p>
          </TooltipContent>
        </Tooltip>
      </TooltipProvider>
      {/* Extension type icon */}
      <PuzzleIcon
        className={cn("size-4", { "text-foreground/50": !isInstalled })}
      />

      {/* Content */}
      <div className="flex flex-col">
        <span
          className={cn("font-medium text-foreground text-sm", {
            "text-foreground/50": !isInstalled,
          })}
        >
          {data.name}
        </span>
        <span
          className={cn("text-muted-foreground/50 text-xs", {
            "text-muted-foreground/20": !isInstalled,
          })}
        >
          {data.addon}
        </span>
      </div>
      {/* Actions */}
      <div className="ml-auto flex items-center gap-1">
        {/* Expand button */}
        <Button
          variant="ghost"
          size="xs"
          className="p-1"
          onClick={onExpandedClick}
        >
          <ChevronDownIcon
            className={cn("size-3", "transition-transform", {
              "rotate-180": isExpanded,
            })}
          />
        </Button>
      </div>
      {children}
    </div>
  );
};

const HandleGroupItem = (props: {
  data: IExtensionNodeData;
  isConnectable: boolean;
  onConnect?: (params: Connection | Edge) => void;
  connectionType: EConnectionType;
}) => {
  const { data, isConnectable, onConnect, connectionType } = props;

  const { appendWidget } = useWidgetStore();

  const handleLaunchConnPopup = (data: {
    source: string;
    target?: string;
    graph: IGraph;
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

      title: <CustomNodeConnPopupTitle source={source} target={target} />,
      metadata: { id, source, target, filters, graph },
    });
  };

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
          // id={`target-${data.name}-${connectionType}`}
          id={data2identifier(EFlowElementIdentifier.HANDLE, {
            type: "target",
            extension: data.name,
            graph: data.graph.uuid,
            connectionType,
          })}
          isConnectable={isConnectable}
          onConnect={onConnect}
          className={cn("size-3")}
        />
        <ConnectionCount
          data={{
            source: data.name,
            target: undefined,
            graph: data.graph,
            metadata: {
              filters: {
                type: connectionType,
                target: true,
              },
            },
          }}
        >
          {connectionType === EConnectionType.CMD && (
            <span>{data.src[connectionType]?.length || 0}</span>
          )}
          {connectionType === EConnectionType.DATA && (
            <span>{data.src[connectionType]?.length || 0}</span>
          )}
          {connectionType === EConnectionType.AUDIO_FRAME && (
            <span>{data.src[connectionType]?.length || 0}</span>
          )}
          {connectionType === EConnectionType.VIDEO_FRAME && (
            <span>{data.src[connectionType]?.length || 0}</span>
          )}
        </ConnectionCount>
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
            source: data.name,
            target: undefined,
            graph: data.graph,
            metadata: {
              filters: {
                type: connectionType,
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
        <ConnectionCount
          data={{
            source: data.name,
            target: undefined,
            graph: data.graph,
            metadata: {
              filters: {
                type: connectionType,
                source: true,
              },
            },
          }}
        >
          {connectionType === EConnectionType.CMD && (
            <span>{data.target[connectionType]?.length || 0}</span>
          )}
          {connectionType === EConnectionType.DATA && (
            <span>{data.target[connectionType]?.length || 0}</span>
          )}
          {connectionType === EConnectionType.AUDIO_FRAME && (
            <span>{data.target[connectionType]?.length || 0}</span>
          )}
          {connectionType === EConnectionType.VIDEO_FRAME && (
            <span>{data.target[connectionType]?.length || 0}</span>
          )}
        </ConnectionCount>
        <BaseHandle
          key={`source-${data.name}-${connectionType}`}
          type="source"
          position={Position.Right}
          // id={`source-${data.name}-${connectionType}`}
          id={data2identifier(EFlowElementIdentifier.HANDLE, {
            type: "source",
            extension: data.name,
            graph: data.graph.uuid,
            connectionType,
          })}
          isConnectable={isConnectable}
          className={cn("size-3")}
        />
      </div>
    </div>
  );
};

const ConnectionCount = (props: {
  children: React.ReactNode;
  data: {
    source: string;
    target?: string;
    graph: IGraph;
    metadata?: {
      filters?: {
        type?: EConnectionType;
        source?: boolean;
        target?: boolean;
      };
    };
  };
}) => {
  const { children, data } = props;

  return (
    <CustomNodeConnectionButton
      data={data}
      size="sm"
      variant="ghost"
      className={cn(
        "w-8 rounded-md px-1 py-0.5 text-center text-xs",
        "cursor-pointer"
      )}
    >
      {children}
    </CustomNodeConnectionButton>
  );
};
