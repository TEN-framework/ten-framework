//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import { ArrowUpFromDotIcon } from "lucide-react";
import { useTranslation } from "react-i18next";
import { GraphPopupTitle } from "@/components/popup/graph";
import {
  CONTAINER_DEFAULT_ID,
  GRAPH_ACTIONS_WIDGET_ID,
  GROUP_GRAPH_ID,
} from "@/constants/widgets";
import {
  ERightClickContextMenuItemType,
  RightClickContextMenuItem,
} from "@/flow/context-menu/base";
import { useWidgetStore } from "@/store";
import type { TSelectorNode } from "@/types/flow";
import { EGraphActions } from "@/types/graphs";
import { EWidgetCategory, EWidgetDisplayType } from "@/types/widgets";

export const ContextMenuItems = (props: { node: TSelectorNode }) => {
  const { node } = props;

  const { t } = useTranslation();
  const { appendWidget } = useWidgetStore();

  // const { data: graphs, mutate: mutateGraphs } = useGraphs();

  const items: RightClickContextMenuItem[] = [
    // {
    //   _type: ERightClickContextMenuItemType.MENU_ITEM,
    //   _id: "selector-node-view-filters",
    //   children: `${t("action.viewDetails")}`,
    //   icon: <ViewIcon />,
    //   disabled: !node.data.filter,
    //   onSelect: () => {
    //     alert(JSON.stringify(node.data.filter, null, 2)); // todo
    //   },
    // },
    {
      _type: ERightClickContextMenuItemType.MENU_ITEM,
      _id: "extension-node-add-connection-from",
      children: t("header.menuGraph.addConnectionFromNode", {
        node: node.data.name,
      }),
      icon: <ArrowUpFromDotIcon />,
      onSelect: () => {
        appendWidget({
          container_id: CONTAINER_DEFAULT_ID,
          group_id: GROUP_GRAPH_ID,
          widget_id:
            GRAPH_ACTIONS_WIDGET_ID +
            `-${EGraphActions.ADD_CONNECTION}-` +
            `${node.data.name}`,

          category: EWidgetCategory.Graph,
          display_type: EWidgetDisplayType.Popup,

          title: <GraphPopupTitle type={EGraphActions.ADD_CONNECTION} />,
          metadata: {
            type: EGraphActions.ADD_CONNECTION,
            graph_id: node.data.graph.graph_id,
            src_node: node,
          },
          popup: {},
        });
      },
    },
    // {
    //   _type: ERightClickContextMenuItemType.SEPARATOR,
    //   _id: "extension-node-separator-1",
    // },
  ];

  return (
    <>
      {items.map((item) => (
        <RightClickContextMenuItem key={item._id} item={item} />
      ))}
    </>
  );
};
