//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { useTranslation } from "react-i18next";
import { BlocksIcon, InfoIcon, PodcastIcon, ScanFaceIcon } from "lucide-react";

import {
  NavigationMenuContent,
  NavigationMenuItem,
  NavigationMenuLink,
  NavigationMenuTrigger,
} from "@/components/ui/NavigationMenu";
import { Separator } from "@/components/ui/Separator";
import { Button } from "@/components/ui/Button";
import { cn } from "@/lib/utils";
import {
  EDefaultWidgetType,
  EWidgetDisplayType,
  EWidgetCategory,
} from "@/types/widgets";
import { useWidgetStore } from "@/store/widget";
import {
  CONTAINER_DEFAULT_ID,
  EXTENSION_STORE_WIDGET_ID,
  DOC_REF_WIDGET_ID,
  GROUP_DOC_REF_ID,
  RTC_INTERACTION_WIDGET_ID,
  TRULIENCE_CONFIG_WIDGET_ID,
} from "@/constants/widgets";
import { EDocLinkKey } from "@/types/doc";
import { ExtensionStorePopupTitle } from "@/components/Popup/Default/Extension";
import { DocRefPopupTitle } from "@/components/Popup/Default/DocRef";

export const RTCInteractionPopupTitle = () => {
  const { t } = useTranslation();
  return t("rtcInteraction.title");
};

export const TrulienceConfigPopupTitle = () => {
  const { t } = useTranslation();
  return t("trulienceConfig.title");
};

export const ExtensionMenu = (props: {
  disableMenuClick?: boolean;
  idx: number;
  triggerListRef?: React.RefObject<HTMLButtonElement[]>;
}) => {
  const { disableMenuClick, idx, triggerListRef } = props;

  const { t } = useTranslation();
  const { appendWidget } = useWidgetStore();

  const onOpenExtensionStore = () => {
    appendWidget({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: EXTENSION_STORE_WIDGET_ID,
      widget_id: EXTENSION_STORE_WIDGET_ID,

      category: EWidgetCategory.Default,
      display_type: EWidgetDisplayType.Popup,

      title: <ExtensionStorePopupTitle />,
      metadata: {
        type: EDefaultWidgetType.ExtensionStore,
      },
      popup: {
        width: 340,
        height: 0.8,
        initialPosition: "top-left",
      },
    });
  };

  const onStartRTCInteraction = () => {
    appendWidget({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: RTC_INTERACTION_WIDGET_ID,
      widget_id: RTC_INTERACTION_WIDGET_ID,

      category: EWidgetCategory.Default,
      display_type: EWidgetDisplayType.Popup,

      title: <RTCInteractionPopupTitle />,
      metadata: {
        type: EDefaultWidgetType.RTCInteraction,
      },
      popup: {
        width: 450,
        height: 520,
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

      title: <TrulienceConfigPopupTitle />,
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

  const openAbout = () => {
    appendWidget({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: GROUP_DOC_REF_ID,
      widget_id: DOC_REF_WIDGET_ID + "-" + EDocLinkKey.Extension,

      category: EWidgetCategory.Default,
      display_type: EWidgetDisplayType.Popup,

      title: <DocRefPopupTitle name={EDocLinkKey.Extension} />,
      metadata: {
        type: EDefaultWidgetType.DocRef,
        doc_link_key: EDocLinkKey.Extension,
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
        {t("header.menuExtension.title")}
      </NavigationMenuTrigger>
      <NavigationMenuContent
        className={cn("flex flex-col items-center px-1 py-1.5 gap-1.5")}
      >
        <NavigationMenuLink asChild>
          <Button
            className="w-full justify-start"
            variant="ghost"
            onClick={onOpenExtensionStore}
          >
            <BlocksIcon />
            {t("header.menuExtension.openExtensionStore")}
          </Button>
        </NavigationMenuLink>
        <NavigationMenuLink asChild>
          <Button
            className="w-full justify-start"
            variant="ghost"
            onClick={onStartRTCInteraction}
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
        <Separator className="w-full" />
        <NavigationMenuLink asChild>
          <Button
            className="w-full justify-start"
            variant="ghost"
            onClick={openAbout}
          >
            <InfoIcon />
            {t("header.menuExtension.about")}
          </Button>
        </NavigationMenuLink>
      </NavigationMenuContent>
    </NavigationMenuItem>
  );
};
