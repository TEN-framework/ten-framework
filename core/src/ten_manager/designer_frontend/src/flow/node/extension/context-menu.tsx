//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import {
  ArrowDownToDotIcon,
  ArrowUpFromDotIcon,
  BrushCleaningIcon,
  FilePenLineIcon,
  LogsIcon,
  ReplaceIcon,
  SaveIcon,
  TablePropertiesIcon,
  TerminalIcon,
  Trash2Icon,
} from "lucide-react";
import * as React from "react";
import { useTranslation } from "react-i18next";
import { toast } from "sonner";
import { postDeleteNode, useGraphs } from "@/api/services/graphs";
import { EditorPopupTitle } from "@/components/popup/editor";
import { GraphPopupTitle } from "@/components/popup/graph";
import { LogViewerPopupTitle } from "@/components/popup/log-viewer";
import {
  CONTAINER_DEFAULT_ID,
  GRAPH_ACTIONS_WIDGET_ID,
  GROUP_EDITOR_ID,
  GROUP_GRAPH_ID,
  GROUP_LOG_VIEWER_ID,
  GROUP_TERMINAL_ID,
} from "@/constants/widgets";
import {
  ERightClickContextMenuItemType,
  RightClickContextMenuItem,
} from "@/flow/context-menu/base";
import { useDialogStore, useWidgetStore } from "@/store";
import type { IExtensionNodeData, TExtensionNode } from "@/types/flow";
import { EGraphActions } from "@/types/graphs";
import {
  ELogViewerScriptType,
  EWidgetCategory,
  EWidgetDisplayType,
  EWidgetPredefinedCheck,
  type IEditorWidgetData,
  type IEditorWidgetRef,
  type ITerminalWidgetData,
} from "@/types/widgets";

export const ContextMenuItems = (props: {
  node: TExtensionNode;
  baseDir: string;
  graphId: string;
}) => {
  const { node, baseDir, graphId } = props;

  const { t } = useTranslation();
  const { appendWidget, removeBackstageWidget, removeLogViewerHistory } =
    useWidgetStore();
  const { appendDialog, removeDialog } = useDialogStore();

  const { data: graphs, mutate: mutateGraphs } = useGraphs();

  const editorRefMappings = React.useRef<
    Record<string, React.RefObject<IEditorWidgetRef>>
  >({});

  const launchEditor = React.useCallback(
    (data: IEditorWidgetData) => {
      const widgetId = `${data.url}-${Date.now()}`;
      appendWidget({
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
          ],
        },
      });
    },
    [appendWidget, t]
  );

  const launchTerminal = React.useCallback(
    (data: ITerminalWidgetData) => {
      const newPopup = { id: `${data.title}-${Date.now()}`, data };
      appendWidget({
        container_id: CONTAINER_DEFAULT_ID,
        group_id: GROUP_TERMINAL_ID,
        widget_id: newPopup.id,

        category: EWidgetCategory.Terminal,
        display_type: EWidgetDisplayType.Popup,

        title: data.title,
        metadata: newPopup.data,
        popup: {
          width: 0.5,
          height: 0.8,
        },
      });
    },
    [appendWidget]
  );

  const launchLogViewer = React.useCallback(
    (data: IExtensionNodeData) => {
      const widgetId = `logViewer-${Date.now()}`;
      appendWidget({
        container_id: CONTAINER_DEFAULT_ID,
        group_id: GROUP_LOG_VIEWER_ID,
        widget_id: widgetId,

        category: EWidgetCategory.LogViewer,
        display_type: EWidgetDisplayType.Popup,

        title: (
          <LogViewerPopupTitle
            title={`${t("popup.logViewer.title")} - ${data.name}`}
          />
        ),
        metadata: {
          wsUrl: "",
          scriptType: ELogViewerScriptType.DEFAULT,
          script: {},
          options: {
            filters: {
              extensions: [data.name],
            },
          },
        },
        popup: {
          width: 0.5,
          height: 0.8,
        },
        actions: {
          onClose: () => {
            removeBackstageWidget(widgetId);
          },
          custom_actions: [
            {
              id: "app-start-log-clean",
              label: t("popup.logViewer.cleanLogs"),
              Icon: BrushCleaningIcon,
              onClick: () => {
                removeLogViewerHistory(widgetId);
              },
            },
          ],
        },
      });
    },
    [appendWidget, removeBackstageWidget, removeLogViewerHistory, t]
  );

  const items: RightClickContextMenuItem[] = [
    {
      _type: ERightClickContextMenuItemType.MENU_SUB,
      _id: "extension-node-edit",
      label: `${t("action.edit")} ${t("extensionStore.extension")}`,
      icon: <FilePenLineIcon />,
      triggerProps: {
        inset: true,
      },
      items: [
        {
          _type: ERightClickContextMenuItemType.MENU_ITEM,
          _id: "extension-node-edit-manifest",
          children: `${t("action.edit")} manifest.json`,
          icon: <FilePenLineIcon />,
          disabled: !node.data.url,
          onSelect: () => {
            console.log("Editing manifest for node:", node);
            if (node.data.url)
              launchEditor({
                title: `${node.data.name} manifest.json`,
                content: "",
                url: `${node.data.url}/manifest.json`,
                refs: editorRefMappings.current,
              });
          },
        },
        {
          _type: ERightClickContextMenuItemType.MENU_ITEM,
          _id: "extension-node-edit-property",
          children: `${t("action.edit")} property.json`,
          icon: <FilePenLineIcon />,
          disabled: !node.data.url,
          onSelect: () => {
            if (node.data.url)
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
      _type: ERightClickContextMenuItemType.SEPARATOR,
      _id: "extension-node-separator-1",
    },
    {
      _type: ERightClickContextMenuItemType.MENU_ITEM,
      _id: "extension-node-update-properties",
      children: `${t("action.update")} ${t("popup.node.properties")}`,
      icon: <TablePropertiesIcon />,
      disabled: !baseDir || !graphId || !node.data.is_installed,
      onSelect: () => {
        if (!baseDir || !graphId) return;
        appendWidget({
          container_id: CONTAINER_DEFAULT_ID,
          group_id: GROUP_GRAPH_ID,
          widget_id: `${GRAPH_ACTIONS_WIDGET_ID}-update-${node.data.name}`,

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
      },
    },
    {
      _type: ERightClickContextMenuItemType.MENU_ITEM,
      _id: "extension-node-add-connection-from",
      children: t("header.menuGraph.addConnectionFromNode", {
        node: node.data.name,
      }),
      icon: <ArrowUpFromDotIcon />,
      disabled: !baseDir || !graphId,
      onSelect: () => {
        if (!baseDir || !graphId) return;
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
            base_dir: baseDir,
            graph_id: graphId,
            src_node: node,
          },
          popup: {},
        });
      },
    },
    {
      _type: ERightClickContextMenuItemType.MENU_ITEM,
      _id: "extension-node-add-connection-to",
      children: t("header.menuGraph.addConnectionToNode", {
        node: node.data.name,
      }),
      icon: <ArrowDownToDotIcon />,
      disabled: !baseDir || !graphId,
      onSelect: () => {
        if (!baseDir || !graphId) return;
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
            base_dir: baseDir,
            graph_id: graphId,
            dest_node: node,
          },
          popup: {},
        });
      },
    },
    {
      _type: ERightClickContextMenuItemType.SEPARATOR,
      _id: "extension-node-separator-2",
    },
    {
      _type: ERightClickContextMenuItemType.MENU_ITEM,
      _id: "extension-node-launch-terminal",
      children: t("action.launchTerminal"),
      icon: <TerminalIcon />,
      onSelect: () => {
        launchTerminal({ title: node.data.name, url: node.data.url });
      },
    },
    {
      _type: ERightClickContextMenuItemType.MENU_ITEM,
      _id: "extension-node-launch-log-viewer",
      children: t("action.launchLogViewer"),
      icon: <LogsIcon />,
      onSelect: () => {
        launchLogViewer(node.data);
      },
    },
    {
      _type: ERightClickContextMenuItemType.SEPARATOR,
      _id: "extension-node-separator-3",
    },
    {
      _type: ERightClickContextMenuItemType.MENU_ITEM,
      _id: "extension-node-replace",
      children: t("action.replaceNode"),
      icon: <ReplaceIcon />,
      disabled: !baseDir || !graphId,
      onSelect: () => {
        const type = EGraphActions.REPLACE_NODE;
        appendWidget({
          container_id: CONTAINER_DEFAULT_ID,
          group_id: GROUP_GRAPH_ID,
          widget_id: `${GRAPH_ACTIONS_WIDGET_ID}-${type}-${baseDir}-${graphId}`,

          category: EWidgetCategory.Graph,
          display_type: EWidgetDisplayType.Popup,

          title: <GraphPopupTitle type={type} node={node} />,
          metadata: {
            type,
            base_dir: baseDir,
            graph_id: graphId,
            node: node,
          },
          popup: {
            width: 340,
          },
        });
      },
    },
    {
      _type: ERightClickContextMenuItemType.MENU_ITEM,
      _id: "extension-node-delete",
      children: t("action.delete"),
      variant: "destructive",
      disabled: !baseDir || !graphId,
      icon: <Trash2Icon />,
      onClick: () => {
        const id = `delete-${node.data._type}-dialog-${node.data.name}`;
        appendDialog({
          id,
          title: t("action.delete"),
          content: t("action.deleteNodeConfirmationWithName", {
            type: node.data._type,
            name: node.data.name,
          }),
          variant: "destructive",
          onCancel: async () => {
            removeDialog(id);
          },
          onConfirm: async () => {
            if (!baseDir || !graphId) {
              removeDialog(id);
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
              const graph = graphs?.find((graph) => graph.graph_id === graphId);
              if (!graph) {
                throw new Error("Graph not found");
              }
              await mutateGraphs();
            } catch (error: unknown) {
              toast.error(t("action.deleteNodeFailed"), {
                description:
                  error instanceof Error ? error.message : "Unknown error",
              });
              console.error(error);
            } finally {
              removeDialog(id);
            }
          },
        });
      },
    },
  ];

  return (
    <>
      {items.map((item) => (
        <RightClickContextMenuItem key={item._id} item={item} />
      ))}
    </>
  );
};
