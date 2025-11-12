//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

// https://reactflow.dev/learn/tutorials/getting-started-with-react-flow-components

import type { HandleProps } from "@xyflow/react";
import { forwardRef, type HTMLAttributes } from "react";
import { BaseHandle } from "@/components/ui/react-flow/base-handle";
import { cn } from "@/lib/utils";

const flexDirections = {
  top: "flex-col",
  right: "flex-row-reverse justify-end",
  bottom: "flex-col-reverse justify-end",
  left: "flex-row",
};

export const LabeledHandle = forwardRef<
  HTMLDivElement,
  HandleProps &
    HTMLAttributes<HTMLDivElement> & {
      title: string;
      handleClassName?: string;
      labelClassName?: string;
    }
>(
  (
    { className, labelClassName, handleClassName, title, position, ...props },
    ref
  ) => (
    <div
      ref={ref}
      title={title}
      className={cn(
        "relative flex items-center",
        flexDirections[position],
        className
      )}
    >
      <BaseHandle position={position} className={handleClassName} {...props} />
      {/** biome-ignore lint/a11y/noLabelWithoutControl: <ignore here> */}
      <label className={cn("px-3 text-foreground", labelClassName)}>
        {title}
      </label>
    </div>
  )
);

LabeledHandle.displayName = "LabeledHandle";
