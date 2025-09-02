//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import {
  GitPullRequestCreateIcon,
  InfoIcon,
  MoveIcon,
  PackagePlusIcon,
} from "lucide-react";
import React from "react";
import { useTranslation } from "react-i18next";
import { useFetchApps } from "@/api/services/apps";
import { DocRefPopupTitle } from "@/components/popup/default/doc-ref";
import { GraphPopupTitle } from "@/components/popup/graph";
import { Button } from "@/components/ui/button";
import {
  NavigationMenuContent,
  NavigationMenuItem,
  NavigationMenuLink,
  NavigationMenuTrigger,
} from "@/components/ui/navigation-menu";

import { Separator } from "@/components/ui/separator";
import {
  CONTAINER_DEFAULT_ID,
  GRAPH_ACTIONS_WIDGET_ID,
  GROUP_DOC_REF_ID,
  GROUP_GRAPH_ID,
} from "@/constants/widgets";
import { cn } from "@/lib/utils";
import { useAppStore, useWidgetStore } from "@/store";
import { EDocLinkKey } from "@/types/doc";
import { EGraphActions } from "@/types/graphs";
import {
  EDefaultWidgetType,
  EWidgetCategory,
  EWidgetDisplayType,
} from "@/types/widgets";

export function GraphMenu(props: {
  onAutoLayout?: () => void;
  disableMenuClick?: boolean;
  idx: number;
  triggerListRef?: React.RefObject<HTMLButtonElement[]>;
}) {
  const { onAutoLayout, disableMenuClick, idx, triggerListRef } = props;

  const { data: appRes } = useFetchApps();

  const { t } = useTranslation();
  const { appendWidget } = useWidgetStore();
  const { selectedGraphs } = useAppStore();

  const [selectedGraph, selectedApp] = React.useMemo(() => {
    if (!selectedGraphs || selectedGraphs.length === 0)
      return [undefined, undefined];
    if (selectedGraphs.length > 1) return [undefined, undefined];
    const targetApp = appRes?.app_info?.find(
      (app) => app.base_dir === selectedGraphs[0].base_dir
    );
    return [selectedGraphs[0], targetApp];
  }, [selectedGraphs, appRes?.app_info]);

  const onGraphAct = (type: EGraphActions) => () => {
    if (!selectedGraph || !selectedApp) return;
    appendWidget({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: GROUP_GRAPH_ID,
      widget_id:
        GRAPH_ACTIONS_WIDGET_ID +
        `-$type-` +
        `$selectedGraph?.base_dir-$selectedGraph?.graph_id`,

      category: EWidgetCategory.Graph,
      display_type: EWidgetDisplayType.Popup,

      title: <GraphPopupTitle type={type} />,
      metadata: {
        type,
        base_dir: selectedGraph?.base_dir,
        graph_id: selectedGraph?.graph_id,
        app_uri: selectedApp?.app_uri,
      },
      popup: {
        width: 340,
      },
    });
  };

  const openAbout = () => {
    appendWidget({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: GROUP_DOC_REF_ID,
      widget_id: `$DOC_REF_WIDGET_ID-$EDocLinkKey.Graph`,

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

  if (
    !selectedGraphs ||
    selectedGraphs.length === 0 ||
    selectedGraphs.length > 1
  ) {
    return null;
  }

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
        className={cn("flex flex-col items-center gap-1.5 px-1 py-1.5")}
      >
        {onAutoLayout && (
          <>
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
          </>
        )}

        <NavigationMenuLink asChild>
          <Button
            className="w-full justify-start"
            variant="ghost"
            disabled={!selectedGraph}
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
            disabled={!selectedGraph}
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
