//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import { ListCollapseIcon, TrashIcon } from "lucide-react";
import type React from "react";
import { useTranslation } from "react-i18next";
import { toast } from "sonner";
import { postDeleteConnection, useGraphs } from "@/api/services/graphs";
// eslint-disable-next-line max-len
import { CustomNodeConnPopupTitle } from "@/components/popup/custom-node-connection";
import {
  CONTAINER_DEFAULT_ID,
  GROUP_CUSTOM_CONNECTION_ID,
} from "@/constants/widgets";
import ContextMenu, {
  EContextMenuItemType,
  type IContextMenuItem,
} from "@/flow/context-menu/base";
import { resetNodesAndEdgesByGraphs } from "@/flow/graph";
import { useDialogStore, useFlowStore, useWidgetStore } from "@/store";
import type { TCustomEdge } from "@/types/flow";
import { EWidgetCategory, EWidgetDisplayType } from "@/types/widgets";

interface EdgeContextMenuProps {
  visible: boolean;
  x: number;
  y: number;
  edge: TCustomEdge;
  onClose: () => void;
}

/** @deprecated */
const EdgeContextMenu: React.FC<EdgeContextMenuProps> = ({
  visible,
  x,
  y,
  edge,
  onClose,
}) => {
  const { t } = useTranslation();

  const { appendDialog, removeDialog } = useDialogStore();
  const { setNodesAndEdges } = useFlowStore();
  const { appendWidget } = useWidgetStore();

  const { data: graphs = [] } = useGraphs();

  const items: IContextMenuItem[] = [
    {
      _type: EContextMenuItemType.BUTTON,
      label: t("action.viewDetails"),
      icon: <ListCollapseIcon />,
      onClick: () => {
        console.log("View details for edge:", edge);
        const { source, target, data } = edge;

        if (!data?.graph) {
          return;
        }

        const id = `${source}-${target ?? ""}`;
        appendWidget({
          container_id: CONTAINER_DEFAULT_ID,
          group_id: GROUP_CUSTOM_CONNECTION_ID,
          widget_id: id,

          category: EWidgetCategory.CustomConnection,
          display_type: EWidgetDisplayType.Popup,

          title: <CustomNodeConnPopupTitle source={source} target={target} />,
          metadata: {
            id,
            source,
            target,
            graph: data.graph,
          },
        });

        onClose();
      },
    },
    // {
    //   _type: EContextMenuItemType.BUTTON,
    //   label: t("action.edit"),
    //   icon: <PencilIcon />,
    //   onClick: () => {
    //     onClose()
    //   },
    // },
    {
      _type: EContextMenuItemType.BUTTON,
      label: t("action.delete"),
      icon: <TrashIcon />,
      onClick: () => {
        const dialogId =
          edge.source +
          edge.target +
          edge.type +
          edge.id +
          "delete-popup-dialog";
        if (!edge?.data?.graph) {
          return;
        }
        appendDialog({
          id: dialogId,
          title: t("action.confirm"),
          content: t("action.deleteConnectionConfirmation"),
          confirmLabel: t("action.delete"),
          cancelLabel: t("action.cancel"),
          onConfirm: async () => {
            if (!edge?.data || !edge?.data?.graph) {
              return;
            }
            try {
              await postDeleteConnection({
                graph_id: edge?.data?.graph?.uuid,
                src_app: edge.data.app,
                src_extension: edge.source,
                msg_type: edge.data.connectionType,
                msg_name: edge.data.name,
                dest_app: edge.data.app,
                dest_extension: edge.target,
              });
              toast.success(t("action.deleteConnectionSuccess"));
              const { nodes, edges } = await resetNodesAndEdgesByGraphs(graphs);
              setNodesAndEdges(nodes, edges);
            } catch (error) {
              console.error(error);
              toast.error(t("action.deleteConnectionFailed"), {
                description:
                  error instanceof Error ? error.message : "Unknown error",
              });
            } finally {
              removeDialog(dialogId);
            }
          },
          onCancel: async () => {
            removeDialog(dialogId);
          },
          postConfirm: async () => {},
        });
        onClose();
      },
    },
  ];

  return <ContextMenu visible={visible} x={x} y={y} items={items} />;
};

export default EdgeContextMenu;
