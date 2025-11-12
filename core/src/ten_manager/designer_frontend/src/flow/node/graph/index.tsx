//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import type { NodeProps } from "@xyflow/react";
import {
  ContextMenu,
  ContextMenuContent,
  ContextMenuTrigger,
} from "@/components/ui/context-menu";
import {
  GroupNode,
  GroupNodeLabel,
} from "@/components/ui/react-flow/labeled-group-node";
import { NODE_CONFIG_MAPPING } from "@/flow/node/base";
import { ContextMenuItems } from "@/flow/node/graph/context-menu";
import { cn } from "@/lib/utils";
import type { TGraphNode } from "@/types/flow";

const CONFIG = NODE_CONFIG_MAPPING.graph;

export const GraphNode = (props: NodeProps<TGraphNode>) => {
  const { data } = props;

  return (
    <ContextMenu>
      <ContextMenuTrigger asChild>
        <GroupNode
          className={cn(
            "border-2 border-gray-300 border-dashed dark:border-gray-600"
          )}
          label={
            <GroupNodeLabel
              className={cn("rounded-br-sm", "flex items-center gap-2")}
            >
              <CONFIG.Icon className="size-4" />
              {data.graph.name}
            </GroupNodeLabel>
          }
        />
      </ContextMenuTrigger>
      <ContextMenuContent className="w-fit">
        <ContextMenuItems graph={data.graph} />
      </ContextMenuContent>
    </ContextMenu>
  );
};
