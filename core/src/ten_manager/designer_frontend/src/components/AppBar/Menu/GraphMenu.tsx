//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { useTranslation } from "react-i18next";
import {
  FolderOpenIcon,
  MoveIcon,
  PackagePlusIcon,
  GitPullRequestCreateIcon,
  InfoIcon,
} from "lucide-react";

import {
  NavigationMenuContent,
  NavigationMenuItem,
  NavigationMenuLink,
  NavigationMenuTrigger,
} from "@/components/ui/NavigationMenu";
import { Separator } from "@/components/ui/Separator";
import { Button } from "@/components/ui/Button";
import { cn } from "@/lib/utils";
import { useWidgetStore, useAppStore } from "@/store";
import {
  CONTAINER_DEFAULT_ID,
  GRAPH_SELECT_WIDGET_ID,
  GROUP_DOC_REF_ID,
  DOC_REF_WIDGET_ID,
  GROUP_GRAPH_ID,
  GRAPH_ACTIONS_WIDGET_ID,
} from "@/constants/widgets";
import {
  EDefaultWidgetType,
  EWidgetCategory,
  EWidgetDisplayType,
} from "@/types/widgets";
import { EGraphActions } from "@/types/graphs";
import { EDocLinkKey } from "@/types/doc";
import { GraphSelectPopupTitle } from "@/components/Popup/Default/GraphSelect";
import { DocRefPopupTitle } from "@/components/Popup/Default/DocRef";
import { GraphPopupTitle } from "@/components/Popup/Graph";

export function GraphMenu(props: {
  onAutoLayout: () => void;
  disableMenuClick?: boolean;
  idx: number;
  triggerListRef?: React.RefObject<HTMLButtonElement[]>;
}) {
  const { onAutoLayout, disableMenuClick, idx, triggerListRef } = props;

  const { t } = useTranslation();
  const { appendWidgetIfNotExists } = useWidgetStore();
  const { currentWorkspace } = useAppStore();

  const onOpenExistingGraph = () => {
    appendWidgetIfNotExists({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: GRAPH_SELECT_WIDGET_ID,
      widget_id: GRAPH_SELECT_WIDGET_ID,

      category: EWidgetCategory.Default,
      display_type: EWidgetDisplayType.Popup,

      title: <GraphSelectPopupTitle />,
      metadata: {
        type: EDefaultWidgetType.GraphSelect,
      },
      popup: {
        width: 0.5,
        height: 0.8,
      },
    });
  };

  const onGraphAct = (type: EGraphActions) => () => {
    if (!currentWorkspace?.graph || !currentWorkspace?.app) return;
    appendWidgetIfNotExists({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: GROUP_GRAPH_ID,
      widget_id:
        GRAPH_ACTIONS_WIDGET_ID +
        `-${type}-` +
        `${currentWorkspace?.app?.base_dir}-${currentWorkspace?.graph?.uuid}`,

      category: EWidgetCategory.Graph,
      display_type: EWidgetDisplayType.Popup,

      title: <GraphPopupTitle type={type} />,
      metadata: {
        type,
        base_dir: currentWorkspace?.app?.base_dir,
        graph_id: currentWorkspace?.graph?.uuid,
        app_uri: currentWorkspace?.app?.app_uri,
      },
      popup: {
        width: 340,
      },
    });
  };

  const openAbout = () => {
    appendWidgetIfNotExists({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: GROUP_DOC_REF_ID,
      widget_id: DOC_REF_WIDGET_ID + "-" + EDocLinkKey.Graph,

      category: EWidgetCategory.Default,
      display_type: EWidgetDisplayType.Popup,

      title: <DocRefPopupTitle name={EDocLinkKey.Graph} />,
      metadata: {
        type: EDefaultWidgetType.DocRef,
        doc_link_key: EDocLinkKey.Graph,
      },
      popup: {
        width: 340,
        height: 0.8,
        initialPosition: "top-left",
      },
    });
  };

  return (
    <NavigationMenuItem>
      <NavigationMenuTrigger
        className="submenu-trigger"
        ref={(ref) => {
          if (triggerListRef?.current && ref) {
            triggerListRef.current[idx] = ref;
          }
        }}
        onClick={(e) => {
          if (disableMenuClick) {
            e.preventDefault();
          }
        }}
      >
        {t("header.menuGraph.title")}
      </NavigationMenuTrigger>
      <NavigationMenuContent
        className={cn("flex flex-col items-center px-1 py-1.5 gap-1.5")}
      >
        <NavigationMenuLink asChild>
          <Button
            className="w-full justify-start"
            variant="ghost"
            onClick={onOpenExistingGraph}
          >
            <FolderOpenIcon />
            {t("header.menuGraph.loadGraph")}
          </Button>
        </NavigationMenuLink>
        <NavigationMenuLink asChild>
          <Button
            className="w-full justify-start"
            variant="ghost"
            onClick={onAutoLayout}
          >
            <MoveIcon />
            {t("header.menuGraph.autoLayout")}
          </Button>
        </NavigationMenuLink>
        <Separator className="w-full" />
        <NavigationMenuLink asChild>
          <Button
            className="w-full justify-start"
            variant="ghost"
            disabled={!currentWorkspace || !currentWorkspace?.graph}
            onClick={onGraphAct(EGraphActions.ADD_NODE)}
          >
            <PackagePlusIcon />
            {t("header.menuGraph.addNode")}
          </Button>
        </NavigationMenuLink>
        <NavigationMenuLink asChild>
          <Button
            className="w-full justify-start"
            variant="ghost"
            disabled={!currentWorkspace || !currentWorkspace?.graph}
            onClick={onGraphAct(EGraphActions.ADD_CONNECTION)}
          >
            <GitPullRequestCreateIcon />
            {t("header.menuGraph.addConnection")}
          </Button>
        </NavigationMenuLink>
        <Separator className="w-full" />
        <NavigationMenuLink asChild>
          <Button
            className="w-full justify-start"
            variant="ghost"
            onClick={openAbout}
          >
            <InfoIcon />
            {t("header.menuGraph.about")}
          </Button>
        </NavigationMenuLink>
      </NavigationMenuContent>
    </NavigationMenuItem>
  );
}
