//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import {
  FolderOpenIcon,
  FolderTreeIcon,
  GitPullRequestCreateIcon,
  PackagePlusIcon,
  PlayIcon,
} from "lucide-react";
import type React from "react";
import { useTranslation } from "react-i18next";
import { useStorage } from "@/api/services/storage";
import ContextMenu, {
  EContextMenuItemType,
  type IContextMenuItem,
} from "@/flow/ContextMenu/ContextMenu";
import type { IRunAppParams } from "@/types/apps";
import { EGraphActions } from "@/types/graphs";

interface PaneContextMenuProps {
  visible: boolean;
  x: number;
  y: number;
  graphId?: string;
  baseDir?: string;
  onOpenExistingGraph?: () => void;
  onGraphAct?: (type: EGraphActions) => void;
  onAppManager?: () => void;
  onAppRun?: (app: IRunAppParams) => void;
  onClose: () => void;
}

const PaneContextMenu: React.FC<PaneContextMenuProps> = ({
  visible,
  x,
  y,
  graphId,
  baseDir,
  onOpenExistingGraph,
  onGraphAct,
  onAppManager,
  onAppRun, // Assuming you have a function to handle running the app
  onClose,
}) => {
  const { t } = useTranslation();
  const { data } = useStorage();
  const { recent_run_apps = [] } = data || {};

  const items: IContextMenuItem[] = [
    {
      _type: EContextMenuItemType.BUTTON,
      label: t("header.menuGraph.loadGraph"),
      icon: <FolderOpenIcon />,
      disabled: !baseDir,
      onClick: () => {
        onClose();
        onOpenExistingGraph?.();
      },
    },
    {
      _type: EContextMenuItemType.SEPARATOR,
    },
    {
      _type: EContextMenuItemType.BUTTON,
      label: t("header.menuGraph.addNode"),
      icon: <PackagePlusIcon />,
      disabled: !graphId,
      onClick: () => {
        onClose();
        onGraphAct?.(EGraphActions.ADD_NODE);
      },
    },
    {
      _type: EContextMenuItemType.BUTTON,
      label: t("header.menuGraph.addConnection"),
      icon: <GitPullRequestCreateIcon />,
      disabled: !graphId,
      onClick: () => {
        onClose();
        onGraphAct?.(EGraphActions.ADD_CONNECTION);
      },
    },
    {
      _type: EContextMenuItemType.SEPARATOR,
    },
    {
      _type: EContextMenuItemType.BUTTON,
      label: t("action.manageApps"),
      icon: <FolderTreeIcon className="size-3" />,
      disabled: !graphId,
      onClick: () => {
        onClose();
        onAppManager?.();
      },
    },
    ...recent_run_apps.map((app: IRunAppParams) => ({
      _type: EContextMenuItemType.BUTTON,
      label: `${t("action.runApp")} ${app.base_dir} ${app.script_name}`,
      icon: <PlayIcon className="size-3" />,
      disabled: !graphId,
      onClick: () => {
        onClose();
        // Assuming you have a function to handle running the app
        // runApp(app);
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
    })),
  ];

  return <ContextMenu visible={visible} x={x} y={y} items={items} />;
};

export default PaneContextMenu;
