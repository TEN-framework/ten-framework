//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import React from "react";
import { useTranslation } from "react-i18next";
import {
  FilePenLineIcon,
  TablePropertiesIcon,
  TerminalIcon,
  Trash2Icon,
  LogsIcon,
  SaveIcon,
  ArrowDownToDotIcon,
  ArrowUpFromDotIcon,
  ReplaceIcon,
  // PinIcon,
} from "lucide-react";
import { toast } from "sonner";

import ContextMenu, {
  EContextMenuItemType,
  type IContextMenuItem,
} from "@/flow/ContextMenu/ContextMenu";
import { useDialogStore, useFlowStore, useWidgetStore } from "@/store";
import { postDeleteNode, useGraphs } from "@/api/services/graphs";
import { resetNodesAndEdgesByGraph } from "@/components/Widget/GraphsWidget";
import {
  EWidgetCategory,
  EWidgetDisplayType,
  type ITerminalWidgetData,
  type IEditorWidgetData,
  type IEditorWidgetRef,
  EWidgetPredefinedCheck,
} from "@/types/widgets";
import { EGraphActions } from "@/types/graphs";

import type { TCustomNode } from "@/types/flow";
import {
  GRAPH_ACTIONS_WIDGET_ID,
  CONTAINER_DEFAULT_ID,
  GROUP_EDITOR_ID,
} from "@/constants/widgets";
import { GROUP_GRAPH_ID } from "@/constants/widgets";
import { GraphPopupTitle } from "@/components/Popup/Graph";
import { EditorPopupTitle } from "@/components/Popup/Editor";

interface NodeContextMenuProps {
  visible: boolean;
  x: number;
  y: number;
  node: TCustomNode;
  baseDir?: string | null;
  graphId?: string | null;
  onClose: () => void;
  onLaunchTerminal: (data: ITerminalWidgetData) => void;
  onLaunchLogViewer?: () => void;
}

const NodeContextMenu: React.FC<NodeContextMenuProps> = ({
  visible,
  x,
  y,
  node,
  baseDir,
  graphId,
  onClose,
  onLaunchTerminal,
  onLaunchLogViewer,
}) => {
  const { t } = useTranslation();
  const { appendDialog, removeDialog } = useDialogStore();
  const { setNodesAndEdges } = useFlowStore();
  const { appendWidgetIfNotExists } = useWidgetStore();

  const { graphs } = useGraphs();

  const editorRefMappings = React.useRef<
    Record<string, React.RefObject<IEditorWidgetRef>>
  >({});

  const launchEditor = (data: IEditorWidgetData) => {
    const widgetId = `${data.url}-${Date.now()}`;
    appendWidgetIfNotExists({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: GROUP_EDITOR_ID,
      widget_id: widgetId,

      category: EWidgetCategory.Editor,
      display_type: EWidgetDisplayType.Popup,

      title: <EditorPopupTitle title={data.title} widgetId={widgetId} />,
      metadata: data,
      popup: {
        width: 0.5,
        height: 0.8,
      },
      actions: {
        checks: [EWidgetPredefinedCheck.EDITOR_UNSAVED_CHANGES],
        custom_actions: [
          {
            id: "save-file",
            label: t("action.save"),
            Icon: SaveIcon,
            onClick: () => {
              editorRefMappings?.current?.[widgetId]?.current?.save?.();
            },
          },
          // {
          //   id: "pin-to-dock",
          //   label: t("action.pinToDock"),
          //   Icon: PinIcon,
          //   onClick: () => {
          //     onClose();
          //     editorRefMappings?.current?.[widgetId]?.current?.check?.({
          //       title: t("action.confirm"),
          //       content: t("popup.editor.confirmSaveChanges"),
          //       postConfirm: async () => {
          //         updateWidgetDisplayType(widgetId, EWidgetDisplayType.Dock);
          //       },
          //     });
          //   },
          // },
        ],
      },
    });
  };

  const items: IContextMenuItem[] = [
    {
      _type: EContextMenuItemType.SUB_MENU_BUTTON,
      label: t("action.edit") + " " + t("extensionStore.extension"),
      icon: <FilePenLineIcon />,
      items: [
        {
          _type: EContextMenuItemType.BUTTON,
          label: t("action.edit") + " manifest.json",
          icon: <FilePenLineIcon />,
          onClick: () => {
            onClose();
            if (node?.data.url)
              launchEditor({
                title: `${node.data.name} manifest.json`,
                content: "",
                url: `${node.data.url}/manifest.json`,
                refs: editorRefMappings.current,
              });
          },
        },
        {
          _type: EContextMenuItemType.BUTTON,
          label: t("action.edit") + " property.json",
          icon: <FilePenLineIcon />,
          onClick: () => {
            onClose();
            if (node?.data.url)
              launchEditor({
                title: `${node.data.name} property.json`,
                content: "",
                url: `${node.data.url}/property.json`,
                refs: editorRefMappings.current,
              });
          },
        },
      ],
    },
    {
      _type: EContextMenuItemType.SEPARATOR,
    },
    {
      _type: EContextMenuItemType.BUTTON,
      label: t("action.update") + " " + t("popup.node.properties"),
      icon: <TablePropertiesIcon />,
      disabled: !baseDir || !graphId,
      onClick: () => {
        if (!baseDir || !graphId) return;
        appendWidgetIfNotExists({
          container_id: CONTAINER_DEFAULT_ID,
          group_id: GROUP_GRAPH_ID,
          widget_id: GRAPH_ACTIONS_WIDGET_ID + `-update-` + node.data.name,

          category: EWidgetCategory.Graph,
          display_type: EWidgetDisplayType.Popup,

          title: (
            <GraphPopupTitle
              type={EGraphActions.UPDATE_NODE_PROPERTY}
              node={node}
            />
          ),
          metadata: {
            type: EGraphActions.UPDATE_NODE_PROPERTY,
            base_dir: baseDir,
            graph_id: graphId,
            node: node,
          },
          popup: {
            width: 340,
            height: 0.8,
          },
        });
        onClose();
      },
    },
    {
      _type: EContextMenuItemType.BUTTON,
      label: t("header.menuGraph.addConnectionFromNode", {
        node: node.data.name,
      }),
      icon: <ArrowUpFromDotIcon />,
      disabled: !baseDir || !graphId,
      onClick: () => {
        if (!baseDir || !graphId) return;
        appendWidgetIfNotExists({
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
            base_dir: baseDir,
            graph_id: graphId,
            node: node,
            src_extension: node.data.name,
          },
          popup: {
            width: 340,
          },
        });
        onClose();
      },
    },
    {
      _type: EContextMenuItemType.BUTTON,
      label: t("header.menuGraph.addConnectionToNode", {
        node: node.data.name,
      }),
      icon: <ArrowDownToDotIcon />,
      disabled: !baseDir || !graphId,
      onClick: () => {
        if (!baseDir || !graphId) return;
        appendWidgetIfNotExists({
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
            base_dir: baseDir,
            graph_id: graphId,
            node: node,
            dest_extension: node.data.name,
          },
          popup: {
            width: 340,
          },
        });
        onClose();
      },
    },
    {
      _type: EContextMenuItemType.SEPARATOR,
    },
    {
      _type: EContextMenuItemType.BUTTON,
      label: t("action.launchTerminal"),
      icon: <TerminalIcon />,
      onClick: () => {
        onClose();
        onLaunchTerminal({ title: node.data.name, url: node.data.url });
      },
    },
    {
      _type: EContextMenuItemType.BUTTON,
      label: t("action.launchLogViewer"),
      icon: <LogsIcon />,
      onClick: () => {
        onClose();
        onLaunchLogViewer?.();
      },
    },
    {
      _type: EContextMenuItemType.SEPARATOR,
    },
    {
      _type: EContextMenuItemType.BUTTON,
      label: t("action.replaceNode"),
      icon: <ReplaceIcon />,
      disabled: !baseDir || !graphId,
      onClick: () => {
        const type = EGraphActions.REPLACE_NODE;
        appendWidgetIfNotExists({
          container_id: CONTAINER_DEFAULT_ID,
          group_id: GROUP_GRAPH_ID,
          widget_id:
            GRAPH_ACTIONS_WIDGET_ID + `-${type}-` + `${baseDir}-${graphId}`,

          category: EWidgetCategory.Graph,
          display_type: EWidgetDisplayType.Popup,

          title: <GraphPopupTitle type={type} node={node} />,
          metadata: {
            type,
            base_dir: baseDir!,
            graph_id: graphId!,
            node: node,
          },
          popup: {
            width: 340,
          },
        });
        onClose();
      },
    },
    {
      _type: EContextMenuItemType.BUTTON,
      label: t("action.delete"),
      disabled: !baseDir || !graphId,
      icon: <Trash2Icon />,
      onClick: () => {
        onClose();
        appendDialog({
          id: "delete-node-dialog-" + node.data.name,
          title: t("action.delete"),
          content: t("action.deleteNodeConfirmationWithName", {
            name: node.data.name,
          }),
          variant: "destructive",
          onCancel: async () => {
            removeDialog("delete-node-dialog-" + node.data.name);
          },
          onConfirm: async () => {
            if (!baseDir || !graphId) {
              removeDialog("delete-node-dialog-" + node.data.name);
              return;
            }
            try {
              await postDeleteNode({
                graph_id: graphId,
                name: node.data.name,
                addon: node.data.addon,
                extension_group: node.data.extension_group,
              });
              toast.success(t("popup.node.deleteNodeSuccess"), {
                description: `${node.data.name}`,
              });
              const graph = graphs?.find((graph) => graph.uuid === graphId);
              if (!graph) {
                throw new Error("Graph not found");
              }
              const { nodes, edges } = await resetNodesAndEdgesByGraph(graph);
              setNodesAndEdges(nodes, edges);
            } catch (error: unknown) {
              toast.error(t("action.deleteNodeFailed"), {
                description:
                  error instanceof Error ? error.message : "Unknown error",
              });
              console.error(error);
            } finally {
              removeDialog("delete-node-dialog-" + node.data.name);
            }
          },
        });
      },
    },
  ];

  return <ContextMenu visible={visible} x={x} y={y} items={items} />;
};

export default NodeContextMenu;
