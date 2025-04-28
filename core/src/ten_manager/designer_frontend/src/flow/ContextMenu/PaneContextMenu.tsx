//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import React from "react";
import { useTranslation } from "react-i18next";
import {
  FolderOpenIcon,
  GitPullRequestCreateIcon,
  PackagePlusIcon,
  // PinIcon,
} from "lucide-react";

import ContextMenu, {
  EContextMenuItemType,
  type IContextMenuItem,
} from "@/flow/ContextMenu/ContextMenu";
import { EGraphActions } from "@/types/graphs";


interface PaneContextMenuProps {
  visible: boolean;
  x: number;
  y: number;
  graphId?: string;
  baseDir?: string;
  onOpenExistingGraph?: () => void;
  onGraphAct?: (type: EGraphActions) => void;
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
  onClose,
}) => {
  const { t } = useTranslation();



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
  ];

  return <ContextMenu visible={visible} x={x} y={y} items={items} />;
};

export default PaneContextMenu;
