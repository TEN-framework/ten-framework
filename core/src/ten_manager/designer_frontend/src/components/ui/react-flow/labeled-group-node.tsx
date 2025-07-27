//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

// https://reactflow.dev/ui/components/labeled-group-node

import { type NodeProps, Panel, type PanelPosition } from "@xyflow/react";
import { forwardRef, type HTMLAttributes, type ReactNode } from "react";
import { BaseNode } from "@/components/ui/react-flow/base-node";
import { cn } from "@/lib/utils";

/* GROUP NODE Label ------------------------------------------------------- */

export type GroupNodeLabelProps = HTMLAttributes<HTMLDivElement>;

export const GroupNodeLabel = forwardRef<HTMLDivElement, GroupNodeLabelProps>(
  ({ children, className, ...props }, ref) => {
    return (
      <div ref={ref} className="h-full w-full" {...props}>
        <div
          className={cn(
            "w-fit bg-secondary p-2 text-card-foreground text-xs",
            className
          )}
        >
          {children}
        </div>
      </div>
    );
  }
);

GroupNodeLabel.displayName = "GroupNodeLabel";

export type GroupNodeProps = Partial<NodeProps> & {
  label?: ReactNode | string;
  position?: PanelPosition;
  className?: string;
};

/* GROUP NODE -------------------------------------------------------------- */

export const GroupNode = forwardRef<HTMLDivElement, GroupNodeProps>(
  ({ label, position, className, ...props }, ref) => {
    const getLabelClassName = (position?: PanelPosition) => {
      switch (position) {
        case "top-left":
          return "rounded-br-sm";
        case "top-center":
          return "rounded-b-sm";
        case "top-right":
          return "rounded-bl-sm";
        case "bottom-left":
          return "rounded-tr-sm";
        case "bottom-right":
          return "rounded-tl-sm";
        case "bottom-center":
          return "rounded-t-sm";
        default:
          return "rounded-br-sm";
      }
    };

    return (
      <BaseNode
        ref={ref}
        className={cn(
          "h-full overflow-hidden rounded-sm bg-card bg-opacity-50 p-0",
          className
        )}
        {...props}
      >
        <Panel className={cn("m-0 p-0")} position={position}>
          {label &&
            (typeof label === "string" ? (
              <GroupNodeLabel className={cn(getLabelClassName(position))}>
                {label}
              </GroupNodeLabel>
            ) : (
              label
            ))}
        </Panel>
      </BaseNode>
    );
  }
);

GroupNode.displayName = "GroupNode";
