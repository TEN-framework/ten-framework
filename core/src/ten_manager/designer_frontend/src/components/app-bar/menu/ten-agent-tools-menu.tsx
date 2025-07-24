//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import { PodcastIcon, ScanFaceIcon } from "lucide-react";
import { useTranslation } from "react-i18next";
// import { Separator } from "@/components/ui/separator";
import { Button } from "@/components/ui/button";
import {
  NavigationMenuContent,
  NavigationMenuItem,
  NavigationMenuLink,
  NavigationMenuTrigger,
} from "@/components/ui/navigation-menu";
import {
  CONTAINER_DEFAULT_ID,
  RTC_INTERACTION_WIDGET_ID,
  TRULIENCE_CONFIG_WIDGET_ID,
} from "@/constants/widgets";
import { cn } from "@/lib/utils";
import { useAppStore } from "@/store";
import { useWidgetStore } from "@/store/widget";
import {
  EDefaultWidgetType,
  EWidgetCategory,
  EWidgetDisplayType,
} from "@/types/widgets";

export const TenAgentToolsMenu = (props: {
  disableMenuClick?: boolean;
  idx: number;
  triggerListRef?: React.RefObject<HTMLButtonElement[]>;
}) => {
  const { disableMenuClick, idx, triggerListRef } = props;

  const { t } = useTranslation();
  const { appendWidget } = useWidgetStore();
  const { selectedGraphs } = useAppStore();

  const onStartRTCInteraction = () => {
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
  };

  const onConfigTrulience = () => {
    appendWidget({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: TRULIENCE_CONFIG_WIDGET_ID,
      widget_id: TRULIENCE_CONFIG_WIDGET_ID,

      category: EWidgetCategory.Default,
      display_type: EWidgetDisplayType.Popup,

      title: t("trulienceConfig.title"),
      metadata: {
        type: EDefaultWidgetType.TrulienceConfig,
      },
      popup: {
        width: 320,
        height: 520,
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
        {t("header.menuTenAgentTools.title")}
      </NavigationMenuTrigger>
      <NavigationMenuContent
        className={cn("flex flex-col items-center gap-1.5 px-1 py-1.5")}
      >
        <NavigationMenuLink asChild>
          <Button
            className="w-full justify-start"
            variant="ghost"
            onClick={onStartRTCInteraction}
            disabled={!selectedGraphs || selectedGraphs?.length === 0}
          >
            <PodcastIcon />
            {t("header.menuExtension.startRTCInteraction")}
          </Button>
        </NavigationMenuLink>
        <NavigationMenuLink asChild>
          <Button
            className="w-full justify-start"
            variant="ghost"
            onClick={onConfigTrulience}
          >
            <ScanFaceIcon />
            {t("header.menuExtension.configTrulienceAvatar")}
          </Button>
        </NavigationMenuLink>
      </NavigationMenuContent>
    </NavigationMenuItem>
  );
};
