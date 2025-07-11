//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import {
  type Node,
  type NodeProps,
  Position,
  useReactFlow,
} from "@xyflow/react";
import { BlocksIcon, SaveOffIcon } from "lucide-react";
import React, { useCallback } from "react";
import { Badge } from "@/components/ui/Badge";
import { Button } from "@/components/ui/Button";
import { DropdownMenuItem } from "@/components/ui/DropdownMenu";
import { BaseNode } from "@/components/ui/react-flow/BaseNode";
import { LabeledHandle } from "@/components/ui/react-flow/LabeledHandle";
import {
  NodeHeader,
  NodeHeaderActions,
  NodeHeaderMenuAction,
  NodeHeaderTitle,
} from "@/components/ui/react-flow/NodeHeader";
import {
  Tooltip,
  TooltipContent,
  TooltipProvider,
  TooltipTrigger,
} from "@/components/ui/Tooltip";
import { cn } from "@/lib/utils";
import type { IExtensionNodeData, TExtensionNode } from "@/types/flow";

export function ExtensionNode({ id, data }: NodeProps<TExtensionNode>) {
  const { updateNodeData, setNodes } = useReactFlow();

  const handleDelete = useCallback(() => {
    setNodes((nodes) => nodes.filter((node) => node.id !== id));
  }, [id, setNodes]);

  return (
    <BaseNode className="h-fit p-0">
      <div className="-top-7 absolute right-2">
        <Badge>
          <BlocksIcon className="me-1 size-3" />
          Ext node
        </Badge>
      </div>
      <NodeHeader
        className={cn(
          "bg-gray-200 dark:bg-gray-800",
          "rounded-t-sm px-2 py-1",
          "text-secondary-foreground"
        )}
      >
        <TooltipProvider delayDuration={50}>
          <Tooltip>
            <TooltipTrigger asChild>
              <Button variant="ghost" size="xs" className="cursor-help">
                <SaveOffIcon className="size-4" />
              </Button>
            </TooltipTrigger>
            <TooltipContent>
              <p>Uninstalled</p>
            </TooltipContent>
          </Tooltip>
        </TooltipProvider>

        <NodeHeaderTitle>Mock title with a long name</NodeHeaderTitle>
        <NodeHeaderActions>
          <NodeHeaderMenuAction
            className="cursor-pointer"
            label="Open node menu"
          >
            <DropdownMenuItem onSelect={handleDelete}>Delete</DropdownMenuItem>
          </NodeHeaderMenuAction>
        </NodeHeaderActions>
      </NodeHeader>

      <div className="my-10 flex items-center gap-2">content here</div>

      <footer className={cn("w-full bg-gray-100", "rounded-b-sm")}>
        <LabeledHandle
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
        />
      </footer>
    </BaseNode>
  );
}
