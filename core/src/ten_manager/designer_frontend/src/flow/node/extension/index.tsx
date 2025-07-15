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
  BlocksIcon,
  DatabaseIcon,
  SaveOffIcon,
  TerminalIcon,
  VideoIcon,
} from "lucide-react";
import React from "react";
import { useTranslation } from "react-i18next";
import { Badge } from "@/components/ui/Badge";
import { Button } from "@/components/ui/Button";
import { BaseHandle } from "@/components/ui/react-flow/BaseHandle";
import { BaseNode } from "@/components/ui/react-flow/BaseNode";
import {
  NodeHeader,
  NodeHeaderActions,
  NodeHeaderMenuAction,
  NodeHeaderTitle,
} from "@/components/ui/react-flow/NodeHeader";
import { Separator } from "@/components/ui/Separator";
import {
  Tooltip,
  TooltipContent,
  TooltipProvider,
  TooltipTrigger,
} from "@/components/ui/Tooltip";
import { ContextMenu } from "@/flow/node/extension/ContextMenu";
import { cn } from "@/lib/utils";
import type { IExtensionNodeData, TExtensionNode } from "@/types/flow";
import { EConnectionType } from "@/types/graphs";
import { dispatchCustomNodeActionPopup } from "@/utils/events";

export function ExtensionNode(props: NodeProps<TExtensionNode>) {
  const { data, isConnectable } = props;

  const { t } = useTranslation();

  const isInstalled = React.useMemo(() => {
    return data.is_installed;
  }, [data.is_installed]);

  return (
    <BaseNode className="h-fit p-0">
      <div className="-top-7 absolute right-2">
        <Badge>
          <BlocksIcon className="me-1 size-3" />
          {t("node.extension.tag")}
        </Badge>
      </div>
      <NodeHeader
        className={cn(
          "bg-gray-200 dark:bg-gray-800",
          "rounded-t-sm px-2 py-1",
          "w-sm",
          "text-secondary-foreground",
          {
            "text-ten-icontext-2": !isInstalled,
          }
        )}
      >
        {!isInstalled && (
          <TooltipProvider delayDuration={50}>
            <Tooltip>
              <TooltipTrigger asChild>
                <Button variant="ghost" size="xs" className="cursor-help">
                  <SaveOffIcon className="size-4" />
                </Button>
              </TooltipTrigger>
              <TooltipContent>
                <p>{t("node.uninstalled")}</p>
              </TooltipContent>
            </Tooltip>
          </TooltipProvider>
        )}
        <div className="font-roboto">
          <NodeHeaderTitle className="text-lg">{data.name}</NodeHeaderTitle>
          <div
            className={cn(
              "text-ten-icontext-2",
              "font-light text-sm leading-none"
            )}
          >
            {data.addon}
          </div>
        </div>
        <NodeHeaderActions>
          <NodeHeaderMenuAction
            className="cursor-pointer"
            label="Open node menu"
          >
            <ContextMenu
              // TODO: Fix type casting
              node={props as unknown as TExtensionNode}
              baseDir={data.graph.base_dir}
              graphId={data.graph.uuid}
            />
          </NodeHeaderMenuAction>
        </NodeHeaderActions>
      </NodeHeader>

      {/* <div className="my-10 flex items-center gap-2">content here</div> */}

      <footer
        className={cn(
          "flex w-full flex-col font-roboto text-xs",
          "rounded-b-sm"
        )}
      >
        {/* <LabeledHandle
          title="CMD"
          type="source"
          position={Position.Right}
          className="mr-1"
        />
        <LabeledHandle
          title="DATA"
          type="source"
          position={Position.Right}
          className="mr-1"
        /> */}
        <Separator className="w-full" />
        <HandleGroupItem
          data={data}
          isConnectable={isConnectable}
          // onConnect={onConnect}
          connectionType={EConnectionType.CMD}
        />
        <Separator className="w-full" />
        <HandleGroupItem
          data={data}
          isConnectable={isConnectable}
          // onConnect={onConnect}
          connectionType={EConnectionType.DATA}
        />
        <Separator className="w-full" />
        <HandleGroupItem
          data={data}
          isConnectable={isConnectable}
          // onConnect={onConnect}
          connectionType={EConnectionType.AUDIO_FRAME}
        />
        <Separator className="w-full" />
        <HandleGroupItem
          data={data}
          isConnectable={isConnectable}
          // onConnect={onConnect}
          connectionType={EConnectionType.VIDEO_FRAME}
        />
      </footer>
    </BaseNode>
  );
}

const HandleGroupItem = (props: {
  data: IExtensionNodeData;
  isConnectable: boolean;
  onConnect?: (params: Connection | Edge) => void;
  connectionType: EConnectionType;
}) => {
  const { data, isConnectable, onConnect, connectionType } = props;

  const handleClickDetails =
    ({
      type,
      source,
      target,
    }: {
      type?: EConnectionType;
      source?: boolean;
      target?: boolean;
    }) =>
    () => {
      dispatchCustomNodeActionPopup({
        action: "connections",
        source: data.name,
        target: undefined,
        metadata: {
          filters: {
            type,
            source,
            target,
          },
        },
      });
    };

  return (
    <div className="my-0.5 flex items-center justify-between gap-x-4">
      <div className="flex items-center gap-x-2">
        <BaseHandle
          key={`target-${data.name}-${connectionType}`}
          type="target"
          position={Position.Left}
          id={`target-${data.name}-${connectionType}`}
          // label={data.name}
          // labelOffsetX={0}
          isConnectable={isConnectable}
          onConnect={onConnect}
        />
        <ConnectionCount
          onClick={handleClickDetails({
            type: connectionType,
            target: true,
          })}
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
        className={cn("flex items-center gap-x-1", {
          "cursor-pointer": handleClickDetails,
        })}
        onClick={handleClickDetails({
          type: connectionType,
        })}
      >
        {connectionType === EConnectionType.CMD && (
          <TerminalIcon className="size-3 shrink-0" />
        )}
        {connectionType === EConnectionType.DATA && (
          <DatabaseIcon className="size-3 shrink-0" />
        )}
        {connectionType === EConnectionType.AUDIO_FRAME && (
          <AudioLinesIcon className="size-3 shrink-0" />
        )}
        {connectionType === EConnectionType.VIDEO_FRAME && (
          <VideoIcon className="size-3 shrink-0" />
        )}
        <span>{connectionType.toUpperCase()}</span>
      </Button>
      <div className="flex items-center gap-x-2">
        <ConnectionCount
          onClick={handleClickDetails({
            type: connectionType,
            source: true,
          })}
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
          id={`source-${data.name}-${connectionType}`}
          // label={data.name}
          // labelOffsetX={0}
          isConnectable={isConnectable}
        />
      </div>
    </div>
  );
};

const ConnectionCount = (props: {
  children: React.ReactNode;
  onClick?: () => void;
}) => {
  const { children, onClick } = props;

  return (
    <Button
      size="sm"
      variant={"ghost"}
      className={cn("w-8 rounded-md bg-muted px-1 py-0.5 text-center text-xs", {
        "cursor-pointer": onClick,
      })}
      onClick={onClick}
    >
      {children}
    </Button>
  );
};
