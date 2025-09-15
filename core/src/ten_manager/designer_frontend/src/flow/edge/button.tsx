//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

// eslint-disable-next-line max-len
import { CustomNodeConnPopupTitle } from "@/components/popup/custom-node-connection";
import { Button, type ButtonProps } from "@/components/ui/button";
import {
  CONTAINER_DEFAULT_ID,
  GROUP_CUSTOM_CONNECTION_ID,
} from "@/constants/widgets";
import { useWidgetStore } from "@/store";
import type { ECustomNodeType } from "@/types/flow";
import type { EConnectionType, GraphInfo } from "@/types/graphs";
import { EWidgetCategory, EWidgetDisplayType } from "@/types/widgets";

export const CustomNodeConnectionButton = (
  props: ButtonProps & {
    data: {
      source: {
        name: string;
        type: ECustomNodeType;
      };
      target?: {
        name: string;
        type: ECustomNodeType;
      };
      graph: GraphInfo;
      metadata?: {
        filters?: {
          type?: EConnectionType;
          source?: boolean;
          target?: boolean;
        };
      };
    };
  }
) => {
  const { onClick, data, ...rest } = props;

  const { appendWidget } = useWidgetStore();

  const handleClick = (event: React.MouseEvent<HTMLButtonElement>) => {
    const { source, target, metadata, graph } = data;
    const id = `${source}-${target ?? ""}`;
    const filters = metadata?.filters;
    appendWidget({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: GROUP_CUSTOM_CONNECTION_ID,
      widget_id: id,

      category: EWidgetCategory.CustomConnection,
      display_type: EWidgetDisplayType.Popup,

      title: (
        <CustomNodeConnPopupTitle source={source.name} target={target?.name} />
      ),
      metadata: { id, source, target, filters, graph },

      popup: {
        height: 0.8,
        width: 0.6,
        maxHeight: 0.8,
        maxWidth: 0.6,
      },
    });
    onClick?.(event);
  };

  return (
    <Button
      onClick={(event) => {
        handleClick(event);
      }}
      {...rest}
    />
  );
};
