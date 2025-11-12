//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

// https://reactflow.dev/learn/tutorials/getting-started-with-react-flow-components

import { Handle, type HandleProps } from "@xyflow/react";
import { forwardRef } from "react";

import { cn } from "@/lib/utils";

export type BaseHandleProps = HandleProps;

export const BaseHandle = forwardRef<HTMLDivElement, BaseHandleProps>(
  ({ className, children, ...props }, ref) => {
    return (
      <Handle
        ref={ref}
        {...props}
        className={cn(
          "border border-slate-300 bg-slate-100",
          "dark:border-secondary dark:bg-secondary",
          "h-[11px] w-[11px] rounded-full transition",
          className
        )}
        {...props}
      >
        {children}
      </Handle>
    );
  }
);

BaseHandle.displayName = "BaseHandle";
