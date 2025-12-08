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
import { ECustomNodeType, type TCustomEdge } from "@/types/flow";

const PARTICLE_COUNT = 6;
const ANIMATE_DURATION = 6;

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
  const is2Selector = React.useMemo(() => {
    return data?.target?.type === ECustomNodeType.SELECTOR;
  }, [data?.target?.type]);

  return (
    <>
      <BaseEdge
        id={id}
        path={path}
        style={{
          ...style,
          strokeWidth: isNames ? 3 : 2,
          ...(is2Selector && {
            strokeDasharray: "5,5",
          }),
        }}
        markerEnd={markerEnd}
      />

      {selected &&
        [...Array(PARTICLE_COUNT)].map((_, i) => (
          <ellipse
            key={`particle-${i}-${ANIMATE_DURATION}`}
            rx="5"
            ry="1.2"
            fill="white"
          >
            <animateMotion
              begin={`${i * (ANIMATE_DURATION / PARTICLE_COUNT)}s`}
              dur={`${ANIMATE_DURATION}s`}
              repeatCount="indefinite"
              rotate="auto"
              path={path}
              calcMode="spline"
              keySplines="0.42, 0, 0.58, 1.0"
            />
          </ellipse>
        ))}
    </>
  );
}
