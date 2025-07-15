//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import type { NodeProps } from "@xyflow/react";
import { WaypointsIcon } from "lucide-react";
import { Badge } from "@/components/ui/Badge";
import { GroupNode } from "@/components/ui/react-flow/LabeledGroupNode";
import type { TGraphNode } from "@/types/flow";

export const GraphNode = (props: NodeProps<TGraphNode>) => {
  const { data } = props;

  return (
    <GroupNode
      label={
        <Badge>
          <WaypointsIcon className="me-1 size-3" />
          {data.graph.name}
        </Badge>
      }
    />
  );
};
