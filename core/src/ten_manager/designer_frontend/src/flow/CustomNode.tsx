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
  DatabaseIcon,
  BlocksIcon as ExtensionIcon,
  TerminalIcon,
  VideoIcon,
} from "lucide-react";
import * as React from "react";

import { Separator } from "@/components/ui/Separator";
import CustomHandle from "@/flow/CustomHandle";
import { cn } from "@/lib/utils";
import type { TCustomNode, TCustomNodeData } from "@/types/flow";
import { EConnectionType } from "@/types/graphs";
import { dispatchCustomNodeActionPopup } from "@/utils/events";

const onConnect = (params: Connection | Edge) =>
  console.log("Handle onConnect", params);

export function CustomNode({ data, isConnectable }: NodeProps<TCustomNode>) {
  return (
    <>
      <div
        className={cn(
          "flex flex-col gap-x-4",
          "max-w-sm items-center rounded-xl bg-popover py-2",
          "shadow-lg outline outline-black/5",
          "dark:-outline-offset-1 dark:shadow-none dark:outline-white/10",
          "font-roboto"
        )}
      >
        <div className="px-4">
          <div className="flex items-center gap-x-2 font-medium text-base">
            <ExtensionIcon className="size-5 shrink-0" />
            <span className="font-medium text-base">{data.name}</span>
          </div>
          <p
            className={cn(
              "text-gray-500 text-xs dark:text-gray-400",
              "font-roboto-condensed"
            )}
          >
            {data.addon}
          </p>
        </div>
        <Separator className="my-2 w-full" />
        <div className="flex w-full flex-col gap-y-0.5 font-roboto text-xs">
          <HandleGroupItem
            data={data}
            isConnectable={isConnectable}
            onConnect={onConnect}
            connectionType={EConnectionType.CMD}
          />
          <Separator className="w-full" />
          <HandleGroupItem
            data={data}
            isConnectable={isConnectable}
            onConnect={onConnect}
            connectionType={EConnectionType.DATA}
          />
          <Separator className="w-full" />
          <HandleGroupItem
            data={data}
            isConnectable={isConnectable}
            onConnect={onConnect}
            connectionType={EConnectionType.AUDIO_FRAME}
          />
          <Separator className="w-full" />
          <HandleGroupItem
            data={data}
            isConnectable={isConnectable}
            onConnect={onConnect}
            connectionType={EConnectionType.VIDEO_FRAME}
          />
        </div>
      </div>
    </>
  );
}

const HandleGroupItem = (props: {
  data: TCustomNodeData;
  isConnectable: boolean;
  onConnect: (params: Connection | Edge) => void;
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
    <div className="flex items-center justify-between gap-x-4">
      <div className="flex items-center gap-x-2">
        <CustomHandle
          key={`target-${data.name}-${connectionType}`}
          type="target"
          position={Position.Left}
          id={`target-${data.name}-${connectionType}`}
          label={data.name}
          labelOffsetX={0}
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
      <div
        className={cn("flex items-center gap-x-1", {
          ["cursor-pointer"]: handleClickDetails,
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
      </div>
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
        <CustomHandle
          key={`source-${data.name}-${connectionType}`}
          type="source"
          position={Position.Right}
          id={`source-${data.name}-${connectionType}`}
          label={data.name}
          labelOffsetX={0}
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
  return (
    <div
      className={cn("w-8 rounded-md bg-muted px-1 py-0.5 text-center text-xs", {
        ["cursor-pointer"]: props.onClick,
      })}
      onClick={props.onClick}
    >
      {props.children}
    </div>
  );
};

export default React.memo(CustomNode);
