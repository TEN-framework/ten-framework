//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as React from "react";
import { useTranslation } from "react-i18next";

import { useWidgetStore } from "@/store/widget";
import {
  GraphAddConnectionWidget,
  GraphAddNodeWidget,
  GraphUpdateNodePropertyWidget,
} from "@/components/Widget/GraphsWidget";
import { EGraphActions } from "@/types/graphs";

import type { IGraphWidget } from "@/types/widgets";
import { TCustomNode } from "@/types/flow";

export const GraphPopupTitle = (props: {
  type: EGraphActions;
  node?: TCustomNode;
}) => {
  const { type, node } = props;
  const { t } = useTranslation();

  const titleMemo = React.useMemo(() => {
    switch (type) {
      case EGraphActions.ADD_NODE:
        return t("popup.graph.titleAddNode");
      case EGraphActions.REPLACE_NODE:
        return t("popup.graph.titleReplaceNode", {
          name: node?.data.name,
        });
      case EGraphActions.ADD_CONNECTION:
        return t("popup.graph.titleAddConnection");
      case EGraphActions.UPDATE_NODE_PROPERTY:
        return t("popup.graph.titleUpdateNodePropertyByName", {
          name: node?.data.name,
        });
      default:
        return t("popup.graph.title");
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [type, node?.data.name]);

  return titleMemo;
};

export const GraphPopupContent = (props: { widget: IGraphWidget }) => {
  const { widget } = props;

  const { removeWidget } = useWidgetStore();

  const { type, node } = widget.metadata;

  return (
    <>
      {type === EGraphActions.ADD_NODE && (
        <GraphAddNodeWidget
          {...widget.metadata}
          postAddNodeActions={() => {
            removeWidget(widget.widget_id);
          }}
        />
      )}
      {type === EGraphActions.REPLACE_NODE && (
        <GraphAddNodeWidget
          {...widget.metadata}
          postAddNodeActions={() => {
            removeWidget(widget.widget_id);
          }}
          isReplaceNode
        />
      )}
      {type === EGraphActions.ADD_CONNECTION && (
        <GraphAddConnectionWidget
          {...widget.metadata}
          postAddConnectionActions={() => {
            removeWidget(widget.widget_id);
          }}
        />
      )}
      {type === EGraphActions.UPDATE_NODE_PROPERTY && node && (
        <GraphUpdateNodePropertyWidget
          {...widget.metadata}
          node={node}
          postUpdateNodePropertyActions={() => {
            removeWidget(widget.widget_id);
          }}
        />
      )}
    </>
  );
};
