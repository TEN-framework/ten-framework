//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import {
  BugPlayIcon,
  FolderTreeIcon,
  MessageSquareShareIcon,
} from "lucide-react";
import * as React from "react";
import { useTranslation } from "react-i18next";
import { toast } from "sonner";
import { useFetchApps } from "@/api/services/apps";
import { LoadedAppsPopupTitle } from "@/components/popup/default/app";
import { Button } from "@/components/ui/button";
import { TEN_FRAMEWORK_DESIGNER_FEEDBACK_ISSUE_URL } from "@/constants";
import {
  APPS_MANAGER_WIDGET_ID,
  CONTAINER_DEFAULT_ID,
} from "@/constants/widgets";
import { cn } from "@/lib/utils";
import { useAppStore, useWidgetStore } from "@/store";
import {
  EDefaultWidgetType,
  EWidgetCategory,
  EWidgetDisplayType,
} from "@/types/widgets";

export default function StatusBar(props: { className?: string }) {
  const { className } = props;

  return (
    <footer
      className={cn(
        "flex select-none items-center justify-between text-xs",
        "h-5 w-full",
        "fixed right-0 bottom-0 left-0",
        "bg-background/80 backdrop-blur-xs",
        "border-[#e5e7eb] border-t dark:border-[#374151]",
        "select-none",
        className
      )}
    >
      <div className="flex h-full w-full gap-2">
        <StatusApps />
        {/* <StatusWorkspace /> */}
      </div>
      <div className="flex w-fit gap-2 px-2">
        <Feedback />
      </div>
    </footer>
  );
}

/** @deprecated */
const StatusApps = () => {
  const { t } = useTranslation();
  const { data, error, isLoading } = useFetchApps();
  const { appendWidget, backstageWidgets } = useWidgetStore();
  const { currentWorkspace, updateCurrentWorkspace } = useAppStore();

  const backstageLogViewerWidgets = React.useMemo(() => {
    return backstageWidgets.filter(
      (w) => w.category === EWidgetCategory.LogViewer
    );
  }, [backstageWidgets]);

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

  React.useEffect(() => {
    if (
      !currentWorkspace?.initialized &&
      !currentWorkspace?.app?.base_dir &&
      data?.app_info?.[0]?.base_dir
    ) {
      updateCurrentWorkspace({
        app: data?.app_info?.[0],
      });
    }
  }, [
    data?.app_info,
    currentWorkspace?.app?.base_dir,
    currentWorkspace?.initialized,
    updateCurrentWorkspace,
  ]);

  React.useEffect(() => {
    if (error) {
      toast.error(t("statusBar.appsError"));
    }
  }, [error, t]);

  if (isLoading || !data) {
    return null;
  }

  return (
    <Button
      variant="ghost"
      size="status"
      className=""
      onClick={openAppsManagerPopup}
    >
      <FolderTreeIcon className="size-3" />
      <span className="">
        {t("statusBar.appsLoadedWithCount", {
          count: data.app_info?.length || 0,
        })}
      </span>
      {backstageLogViewerWidgets.length > 0 && (
        <>
          <BugPlayIcon className="ms-1 size-3" />
          <span className="text-muted-foreground text-xs">
            {t("statusBar.scriptsRunningWithCount", {
              count: backstageLogViewerWidgets.length,
            })}
          </span>
        </>
      )}
    </Button>
  );
};

const Feedback = () => {
  const { t } = useTranslation();

  return (
    <Button asChild variant="ghost" size="status" className="truncate">
      <a
        target="_blank"
        referrerPolicy="no-referrer"
        href={TEN_FRAMEWORK_DESIGNER_FEEDBACK_ISSUE_URL}
        className="animate-[pulse_1s_ease-in-out_5]"
      >
        <MessageSquareShareIcon className="size-3" />
        <span>{t("statusBar.feedback.title")}</span>
      </a>
    </Button>
  );
};
