//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { InfoIcon, SettingsIcon } from "lucide-react";
import { useTranslation } from "react-i18next";
import { AboutWidgetTitle } from "@/components/popup/default/about";
import { PreferencesWidgetTitle } from "@/components/popup/default/preferences";
import { Button } from "@/components/ui/button";
import {
  NavigationMenuContent,
  NavigationMenuItem,
  NavigationMenuLink,
  NavigationMenuTrigger,
} from "@/components/ui/navigation-menu";
import { Separator } from "@/components/ui/separator";
import {
  ABOUT_WIDGET_ID,
  CONTAINER_DEFAULT_ID,
  GROUP_ABOUT_ID,
  PREFERENCES_WIDGET_ID,
} from "@/constants/widgets";
import { cn } from "@/lib/utils";
import { useWidgetStore } from "@/store/widget";
import {
  EDefaultWidgetType,
  EWidgetCategory,
  EWidgetDisplayType,
} from "@/types/widgets";

export function DesignerMenu(props: {
  disableMenuClick?: boolean;
  idx: number;
  triggerListRef?: React.RefObject<HTMLButtonElement[]>;
}) {
  const { disableMenuClick, idx, triggerListRef } = props;

  const { t } = useTranslation();

  const { appendWidget } = useWidgetStore();

  const openAbout = () => {
    appendWidget({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: GROUP_ABOUT_ID,
      widget_id: ABOUT_WIDGET_ID,

      category: EWidgetCategory.Default,
      display_type: EWidgetDisplayType.Popup,

      title: <AboutWidgetTitle />,
      metadata: {
        type: EDefaultWidgetType.About,
      },
    });
  };

  const openPreferences = () => {
    appendWidget({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: PREFERENCES_WIDGET_ID,
      widget_id: PREFERENCES_WIDGET_ID,

      category: EWidgetCategory.Default,
      display_type: EWidgetDisplayType.Popup,

      title: <PreferencesWidgetTitle />,
      metadata: {
        type: EDefaultWidgetType.Preferences,
      },
    });
  };

  return (
    <NavigationMenuItem>
      <NavigationMenuTrigger
        className="submenu-trigger font-bold"
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
        {t("header.menuDesigner.title")}
      </NavigationMenuTrigger>
      <NavigationMenuContent
        className={cn("flex flex-col items-center gap-1.5 px-1 py-1.5")}
      >
        <NavigationMenuLink asChild>
          <Button
            className="w-full max-w-(--breakpoint-sm) justify-start"
            variant="ghost"
            onClick={openPreferences}
          >
            <SettingsIcon />
            {t("header.menuDesigner.preferences")}
          </Button>
        </NavigationMenuLink>
        <Separator className="w-full" />
        <NavigationMenuLink asChild>
          <Button
            className="w-full max-w-(--breakpoint-sm) justify-start"
            variant="ghost"
            onClick={openAbout}
          >
            <InfoIcon />
            {t("header.menuDesigner.about")}
          </Button>
        </NavigationMenuLink>
      </NavigationMenuContent>
    </NavigationMenuItem>
  );
}
