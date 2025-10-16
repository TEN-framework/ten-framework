//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import type { NodeProps } from "@xyflow/react";
import { BaseNode } from "@/components/ui/react-flow/base-node";
import { Separator } from "@/components/ui/separator";
import { NODE_CONFIG_MAPPING } from "@/flow/node/base";
import { cn } from "@/lib/utils";
import type { TSubGraphNode } from "@/types/flow";

const CONFIG = NODE_CONFIG_MAPPING.subgraph;

export function SubgraphNode(props: NodeProps<TSubGraphNode>) {
  const { data } = props;

  return (
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
            {data.type}
          </span>
          <span className={cn("text-muted-foreground/50 text-xs")}>
            {data.type}
          </span>
        </div>
      </div>
      {/* Connection handles section */}
      <Separator />
      <div className={cn("py-1")}>
        {/* <div className="space-y-1">
          <HandleGroupItem data={data} connectionType={EConnectionType.CMD} />
          <Separator className={cn("my-1")} />
          <HandleGroupItem data={data} connectionType={EConnectionType.DATA} />
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
        </div> */}
      </div>
    </BaseNode>
  );
}
