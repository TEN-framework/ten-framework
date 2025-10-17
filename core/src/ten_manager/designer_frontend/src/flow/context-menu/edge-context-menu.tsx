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
import { useDialogStore, useWidgetStore } from "@/store";
import { ECustomNodeType, type TCustomEdge } from "@/types/flow";
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
  const { appendWidget } = useWidgetStore();

  const { mutate: mutateGraphs } = useGraphs();

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
            source: {
              type: data.source.type,
              name: data.source.name,
            },
            target: {
              type: data.target.type,
              name: data.target.name,
            },
            graph: data.graph,
          },

          popup: {
            height: 0.8,
            width: 0.6,
            maxHeight: 0.8,
            maxWidth: 0.6,
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
      className: "text-destructive",
      label: t("action.delete"),
      icon: <TrashIcon />,
      disabled: edge.data?.target?.type === ECustomNodeType.SELECTOR,
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
          variant: "destructive",
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
                graph_id: edge?.data?.graph?.graph_id,
                src: {
                  app: edge.data.app,
                  [edge.data.source.type]: edge.data.source.name,
                },
                dest: {
                  app: edge.data.app,
                  [edge.data.target.type]: edge.data.target.name,
                },
                msg_type: edge.data.connectionType,
                msg_names: edge.data.names || [edge.data.name],
              });
              toast.success(t("action.deleteConnectionSuccess"));
              await mutateGraphs();
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
