//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import {
  FolderCogIcon,
  FolderOpenIcon,
  FolderPlusIcon,
  FolderSyncIcon,
  InfoIcon,
} from "lucide-react";
import { useTranslation } from "react-i18next";
import { toast } from "sonner";
import { postReloadApps, useFetchApps } from "@/api/services/apps";
import { useGraphs } from "@/api/services/graphs";
import {
  AppCreatePopupTitle,
  AppFolderPopupTitle,
  LoadedAppsPopupTitle,
} from "@/components/popup/default/app";
import { DocRefPopupTitle } from "@/components/popup/default/doc-ref";
import { Button } from "@/components/ui/button";
import {
  NavigationMenuContent,
  NavigationMenuItem,
  NavigationMenuLink,
  NavigationMenuTrigger,
} from "@/components/ui/navigation-menu";
import { Separator } from "@/components/ui/separator";
import {
  APP_CREATE_WIDGET_ID,
  APP_FOLDER_WIDGET_ID,
  APPS_MANAGER_WIDGET_ID,
  CONTAINER_DEFAULT_ID,
  DOC_REF_WIDGET_ID,
  GROUP_DOC_REF_ID,
} from "@/constants/widgets";
import { cn } from "@/lib/utils";
import { useDialogStore, useWidgetStore } from "@/store";
import { EDocLinkKey } from "@/types/doc";
import {
  EDefaultWidgetType,
  EWidgetCategory,
  EWidgetDisplayType,
} from "@/types/widgets";

export function AppMenu(props: {
  disableMenuClick?: boolean;
  idx: number;
  triggerListRef?: React.RefObject<HTMLButtonElement[]>;
}) {
  const { disableMenuClick, idx, triggerListRef } = props;

  const { t } = useTranslation();

  const { appendWidget } = useWidgetStore();
  const { appendDialog, removeDialog } = useDialogStore();

  const { mutate: mutateApps } = useFetchApps();
  const { mutate: mutateGraphs } = useGraphs();

  const openAppFolderPopup = () => {
    appendWidget({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: APP_FOLDER_WIDGET_ID,
      widget_id: APP_FOLDER_WIDGET_ID,

      category: EWidgetCategory.Default,
      display_type: EWidgetDisplayType.Popup,

      title: <AppFolderPopupTitle />,
      metadata: {
        type: EDefaultWidgetType.AppFolder,
      },
      popup: {
        width: 0.5,
        height: 0.8,
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

  const openAppCreatePopup = () => {
    appendWidget({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: APP_CREATE_WIDGET_ID,
      widget_id: APP_CREATE_WIDGET_ID,

      category: EWidgetCategory.Default,
      display_type: EWidgetDisplayType.Popup,

      title: <AppCreatePopupTitle />,
      metadata: {
        type: EDefaultWidgetType.AppCreate,
      },
    });
  };

  const openAbout = () => {
    appendWidget({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: GROUP_DOC_REF_ID,
      widget_id: `${DOC_REF_WIDGET_ID}-${EDocLinkKey.App}`,

      category: EWidgetCategory.Default,
      display_type: EWidgetDisplayType.Popup,

      title: <DocRefPopupTitle name={EDocLinkKey.App} />,
      metadata: {
        type: EDefaultWidgetType.DocRef,
        doc_link_key: EDocLinkKey.App,
      },
      popup: {
        width: 340,
        height: 0.8,
        initialPosition: "top-left",
      },
    });
  };

  const reloadApps = async (baseDir?: string) => {
    try {
      await postReloadApps(baseDir);
      if (baseDir) {
        toast.success(t("header.menuApp.reloadAppSuccess"));
      } else {
        toast.success(t("header.menuApp.reloadAllAppsSuccess"));
      }
    } catch (error) {
      console.error(error);
      if (baseDir) {
        toast.error(
          t("header.menuApp.reloadAppFailed", {
            description:
              error instanceof Error ? error.message : t("popup.apps.error"),
          })
        );
      } else {
        toast.error(
          t("header.menuApp.reloadAllAppsFailed", {
            description:
              error instanceof Error ? error.message : t("popup.apps.error"),
          })
        );
      }
    } finally {
      await mutateApps();
      await mutateGraphs();
    }
  };

  const handleReloadApp = async (baseDir?: string) => {
    appendDialog({
      id: "reload-app",
      title: baseDir
        ? t("header.menuApp.reloadApp")
        : t("header.menuApp.reloadAllApps"),
      content: (
        <div className={cn("flex flex-col gap-2", "text-sm")}>
          <p className="">
            {baseDir
              ? t("header.menuApp.reloadAppConfirmation", {
                  name: baseDir,
                })
              : t("header.menuApp.reloadAllAppsConfirmation")}
          </p>
          <p>{t("header.menuApp.reloadAppDescription")}</p>
        </div>
      ),
      onCancel: async () => {
        removeDialog("reload-app");
      },
      onConfirm: async () => {
        await reloadApps(baseDir);
        removeDialog("reload-app");
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
        {t("header.menuApp.title")}
      </NavigationMenuTrigger>
      <NavigationMenuContent
        className={cn("flex flex-col items-center gap-1.5 px-1 py-1.5")}
      >
        <NavigationMenuLink asChild>
          <Button
            className="w-full justify-start"
            variant="ghost"
            onClick={openAppCreatePopup}
          >
            <FolderPlusIcon className="me-2 h-4 w-4" />
            {t("header.menuApp.createApp")}
          </Button>
        </NavigationMenuLink>
        <NavigationMenuLink asChild>
          <Button
            className="w-full justify-start"
            variant="ghost"
            onClick={openAppFolderPopup}
          >
            <FolderOpenIcon className="me-2 h-4 w-4" />
            {t("header.menuApp.loadApp")}
          </Button>
        </NavigationMenuLink>
        <NavigationMenuLink asChild>
          <Button
            className="w-full justify-start"
            variant="ghost"
            onClick={() => {
              handleReloadApp();
            }}
          >
            <FolderSyncIcon className="me-2 h-4 w-4" />
            {t("header.menuApp.reloadAllApps")}
          </Button>
        </NavigationMenuLink>
        <NavigationMenuLink asChild>
          <Button
            className="w-full justify-start"
            variant="ghost"
            onClick={openAppsManagerPopup}
          >
            <FolderCogIcon className="me-2 h-4 w-4" />
            {t("header.menuApp.manageLoadedApps")}
          </Button>
        </NavigationMenuLink>
        <Separator className="w-full" />
        <NavigationMenuLink asChild>
          <Button
            className="w-full justify-start"
            variant="ghost"
            onClick={openAbout}
          >
            <InfoIcon className="me-2 h-4 w-4" />
            {t("header.menuApp.about")}
          </Button>
        </NavigationMenuLink>
      </NavigationMenuContent>
    </NavigationMenuItem>
  );
}
