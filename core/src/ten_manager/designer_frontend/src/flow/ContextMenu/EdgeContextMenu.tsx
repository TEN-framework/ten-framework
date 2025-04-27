//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import React from "react";
import { useTranslation } from "react-i18next";
import { PencilIcon, ListCollapseIcon, TrashIcon } from "lucide-react";
import { toast } from "sonner";

import ContextMenu, {
  type IContextMenuItem,
  EContextMenuItemType,
} from "@/flow/ContextMenu/ContextMenu";
import { dispatchCustomNodeActionPopup } from "@/utils/events";
import { useDialogStore, useAppStore, useFlowStore } from "@/store";
import { resetNodesAndEdgesByGraph } from "@/components/Widget/GraphsWidget";
import { postDeleteConnection } from "@/api/services/graphs";

import type { TCustomEdge } from "@/types/flow";

interface EdgeContextMenuProps {
  visible: boolean;
  x: number;
  y: number;
  edge: TCustomEdge;
  onClose: () => void;
}

const EdgeContextMenu: React.FC<EdgeContextMenuProps> = ({
  visible,
  x,
  y,
  edge,
  onClose,
}) => {
  const { t } = useTranslation();

  const { appendDialog, removeDialog } = useDialogStore();
  const { currentWorkspace } = useAppStore();
  const { setNodesAndEdges } = useFlowStore();

  const items: IContextMenuItem[] = [
    {
      _type: EContextMenuItemType.BUTTON,
      label: t("action.viewDetails"),
      icon: <ListCollapseIcon />,
      onClick: () => {
        dispatchCustomNodeActionPopup({
          action: "connections",
          source: edge.source,
          target: edge.target,
        });
        onClose();
      },
    },
    {
      _type: EContextMenuItemType.BUTTON,
      label: t("action.edit"),
      icon: <PencilIcon />,
      onClick: () => {
        onClose();
      },
    },
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
        if (!currentWorkspace?.graph) {
          return;
        }
        appendDialog({
          id: dialogId,
          title: t("action.confirm"),
          content: t("action.deleteConnectionConfirmation"),
          confirmLabel: t("action.delete"),
          cancelLabel: t("action.cancel"),
          onConfirm: async () => {
            try {
              await postDeleteConnection({
                graph_id: currentWorkspace!.graph!.uuid,
                src_app: edge.data!.app,
                src_extension: edge.source,
                msg_type: edge.data!.connectionType,
                msg_name: edge.data!.name,
                dest_app: edge.data!.app,
                dest_extension: edge.target,
              });
              toast.success(t("action.deleteConnectionSuccess"));
              const { nodes, edges } = await resetNodesAndEdgesByGraph(
                currentWorkspace!.graph!
              );
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
