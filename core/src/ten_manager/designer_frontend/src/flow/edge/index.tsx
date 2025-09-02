//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import {
  BaseEdge,
  type EdgeProps,
  getSmoothStepPath,
  Position,
} from "@xyflow/react";
import * as React from "react";
import type { TCustomEdge } from "@/types/flow";

export function CustomEdge({
  sourceX,
  sourceY,
  targetX,
  targetY,
  id,
  style,
  selected,
  markerEnd,
  data,
}: EdgeProps<TCustomEdge>) {
  const [path] = getSmoothStepPath({
    sourceX: sourceX,
    sourceY: sourceY,
    sourcePosition: Position.Right,
    targetX: targetX,
    targetY: targetY,
    targetPosition: Position.Left,
  });

  const isNames = React.useMemo(() => {
    return data?.names?.length && data?.names?.length > 0;
  }, [data?.names?.length]);

  return (
    <>
      <BaseEdge
        id={id}
        path={path}
        style={{ ...style, strokeWidth: isNames ? 3 : 2 }}
        markerEnd={markerEnd}
      />

      {selected && (
        <path
          id={id}
          d={path}
          fill="none"
          strokeDasharray="5,5"
          stroke="url(#edge-gradient)"
          strokeWidth={isNames ? 3 : 2}
          opacity="0.75"
        >
          <animate
            attributeName="stroke-dashoffset"
            values="100;0"
            dur="0.75s"
            repeatCount="indefinite"
          />
        </path>
      )}
    </>
  );
}
