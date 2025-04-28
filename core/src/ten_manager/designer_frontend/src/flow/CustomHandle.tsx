//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import React from "react";
import { Handle, Position, HandleProps } from "@xyflow/react";

interface CustomHandleProps extends HandleProps {
  id: string;
  type: "source" | "target";
  position: Position;
  label: string;
  labelOffsetX?: number;
  labelOffsetY?: number;
  style?: React.CSSProperties;
}

const CustomHandle: React.FC<CustomHandleProps> = ({
  id,
  type,
  position,
  style = {},
}) => {
  return (
    <div className="relative">
      {/* Render the actual handle */}
      <Handle id={id} type={type} position={position} style={style} />
    </div>
  );
};

export default CustomHandle;
