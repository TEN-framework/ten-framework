//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import {
  BrushCleaningIcon,
  FolderTreeIcon,
  GitPullRequestCreateIcon,
  PackagePlusIcon,
  PlayIcon,
} from "lucide-react";
import { useTranslation } from "react-i18next";
import { useFetchApps } from "@/api/services/apps";
import {
  addRecentRunApp as addToRecentRunApp,
  useStorage,
} from "@/api/services/storage";
import { LoadedAppsPopupTitle } from "@/components/popup/default/app";
import { GraphPopupTitle } from "@/components/popup/graph";
import { LogViewerPopupTitle } from "@/components/popup/log-viewer";
import { TEN_PATH_WS_EXEC } from "@/constants";
import { getWSEndpointFromWindow } from "@/constants/utils";
import {
  APPS_MANAGER_WIDGET_ID,
  CONTAINER_DEFAULT_ID,
  GRAPH_ACTIONS_WIDGET_ID,
  GROUP_GRAPH_ID,
  GROUP_LOG_VIEWER_ID,
  RTC_INTERACTION_WIDGET_ID,
} from "@/constants/widgets";
import {
  ERightClickContextMenuItemType,
  RightClickContextMenuItem,
} from "@/flow/context-menu/base";
import { useWidgetStore } from "@/store";
import type { IRunAppParams } from "@/types/apps";
import { EGraphActions, type GraphInfo } from "@/types/graphs";
import {
  EDefaultWidgetType,
  ELogViewerScriptType,
  EWidgetCategory,
  EWidgetDisplayType,
} from "@/types/widgets";

export const ContextMenuItems = (props: { graph: GraphInfo }) => {
  const { graph } = props;

  const { t } = useTranslation();
  const { appendWidget, removeBackstageWidget, removeLogViewerHistory } =
    useWidgetStore();

  const { data: apps } = useFetchApps();

  const { data } = useStorage();
  const { recent_run_apps = [] } =
    (data as { recent_run_apps: IRunAppParams[] }) || {};

  const onGraphAct = (type: EGraphActions) => {
    appendWidget({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: GROUP_GRAPH_ID,
      widget_id:
        GRAPH_ACTIONS_WIDGET_ID +
        `-${type}-` +
        `${graph?.base_dir}-${graph?.graph_id}`,

      category: EWidgetCategory.Graph,
      display_type: EWidgetDisplayType.Popup,

      title: <GraphPopupTitle type={type} />,
      metadata: {
        type,
        base_dir: graph?.base_dir,
        graph_id: graph?.graph_id,
        app_uri: apps?.app_info?.find((app) => app.base_dir === graph?.base_dir)
          ?.app_uri,
      },
      popup: {
        width: 340,
      },
    });
  };

  const openAppsManagerPopup = () => {
    appendWidget({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: APPS_MANAGER_WIDGET_ID,
      widget_id: APPS_MANAGER_WIDGET_ID,

      category: EWidgetCategory.Default,
      display_type: EWidgetDisplayType.Popup,

      title: <LoadedAppsPopupTitle />,
      metadata: {
        type: EDefaultWidgetType.AppsManager,
      },
      popup: {
        width: 0.5,
        height: 0.8,
        maxWidth: 800,
      },
    });
  };

  const onAppRun = async ({
    base_dir,
    script_name,
    run_with_agent,
    stderr_is_log,
    stdout_is_log,
  }: IRunAppParams) => {
    const newAppStartWidgetId = `app-start-${Date.now()}`;

    await addToRecentRunApp({
      base_dir: base_dir,
      script_name: script_name,
      stdout_is_log: stdout_is_log,
      stderr_is_log: stderr_is_log,
      run_with_agent: run_with_agent,
    });

    appendWidget({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: GROUP_LOG_VIEWER_ID,
      widget_id: newAppStartWidgetId,

      category: EWidgetCategory.LogViewer,
      display_type: EWidgetDisplayType.Popup,

      title: <LogViewerPopupTitle />,
      metadata: {
        wsUrl: getWSEndpointFromWindow() + TEN_PATH_WS_EXEC,
        scriptType: ELogViewerScriptType.RUN_SCRIPT,
        script: {
          type: ELogViewerScriptType.RUN_SCRIPT,
          base_dir: base_dir,
          name: script_name,
          stdout_is_log: stdout_is_log,
          stderr_is_log: stderr_is_log,
        },
      },
      popup: {
        width: 0.5,
        height: 0.8,
      },
      actions: {
        onClose: () => {
          removeBackstageWidget(newAppStartWidgetId);
        },
        custom_actions: [
          {
            id: "app-start-log-clean",
            label: t("popup.logViewer.cleanLogs"),
            Icon: BrushCleaningIcon,
            onClick: () => {
              removeLogViewerHistory(newAppStartWidgetId);
            },
          },
        ],
      },
    });

    if (run_with_agent) {
      appendWidget({
        container_id: CONTAINER_DEFAULT_ID,
        group_id: RTC_INTERACTION_WIDGET_ID,
        widget_id: RTC_INTERACTION_WIDGET_ID,

        category: EWidgetCategory.Default,
        display_type: EWidgetDisplayType.Popup,

        title: t("rtcInteraction.title"),
        metadata: {
          type: EDefaultWidgetType.RTCInteraction,
        },
        popup: {
          width: 450,
          height: 700,
          initialPosition: "top-left",
        },
      });
    }
  };

  const items: RightClickContextMenuItem[] = [
    {
      _id: "viewDetails",
      _type: ERightClickContextMenuItemType.MENU_ITEM,
      children: t("header.menuGraph.addNode"),
      icon: <PackagePlusIcon />,
      onClick: () => {
        // onClose()
        onGraphAct(EGraphActions.ADD_NODE);
      },
    },
    {
      _id: "editNode",
      _type: ERightClickContextMenuItemType.MENU_ITEM,
      children: t("header.menuGraph.addConnection"),
      icon: <GitPullRequestCreateIcon />,
      onClick: () => {
        onGraphAct(EGraphActions.ADD_CONNECTION);
      },
    },
    {
      _id: "separator1",
      _type: ERightClickContextMenuItemType.SEPARATOR,
    },
    {
      _id: "manageApps",
      _type: ERightClickContextMenuItemType.MENU_ITEM,
      children: t("action.manageApps"),
      icon: <FolderTreeIcon />,
      onClick: () => {
        openAppsManagerPopup();
      },
    },
    ...recent_run_apps.map(
      (app: IRunAppParams) =>
        ({
          _id: `runApp-${app.script_name}-${app.base_dir}`,
          _type: ERightClickContextMenuItemType.MENU_ITEM,
          children: `${t("action.runApp")} ${app.base_dir} ${app.script_name}`,
          icon: <PlayIcon />,
          onClick: () => {
            // Assuming you have a function to handle running the app
            // runApp(app)
            onAppRun?.({
              script_name: app.script_name,
              base_dir: app.base_dir,
              // Assuming default value, adjust as needed
              run_with_agent: app.run_with_agent,
              // Assuming default value, adjust as needed
              stderr_is_log: true,
              // Assuming default value, adjust as needed
              stdout_is_log: true,
            });
          },
        }) as RightClickContextMenuItem
    ),
  ];

  return (
    <>
      {items.map((item) => (
        <RightClickContextMenuItem key={item._id} item={item} />
      ))}
    </>
  );
};
