//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import { PlayIcon } from "lucide-react";
import * as React from "react";
import { useTranslation } from "react-i18next";
import { toast } from "sonner";
import { postLoadDir, useFetchApps } from "@/api/services/apps";
import { useGraphs } from "@/api/services/graphs";
import { addRecentRunApp } from "@/api/services/storage";
import { AppFileManager } from "@/components/file-manager/app-folder";
import { LogViewerPopupTitle } from "@/components/popup/log-viewer";
import { Button } from "@/components/ui/button";
import { Checkbox } from "@/components/ui/checkbox";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import {
  Select,
  SelectContent,
  SelectGroup,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import { Separator } from "@/components/ui/separator";
import {
  AppsManagerWidget,
  AppTemplateWidget,
} from "@/components/widget/apps-widget";
import { TEN_PATH_WS_EXEC } from "@/constants";
import { getWSEndpointFromWindow } from "@/constants/utils";
import {
  CONTAINER_DEFAULT_ID,
  GROUP_LOG_VIEWER_ID,
  RTC_INTERACTION_WIDGET_ID,
} from "@/constants/widgets";
import { EWidgetIdentifier } from "@/lib/identifier";
import { useAppStore, useWidgetStore } from "@/store";
import {
  EDefaultWidgetType,
  ELogViewerScriptType,
  EWidgetCategory,
  EWidgetDisplayType,
  type IDefaultWidget,
  type IDefaultWidgetData,
  type IWidget,
} from "@/types/widgets";

export const AppFolderPopupTitle = () => {
  const { t } = useTranslation();
  return t("header.menuApp.openAppFolder");
};

export const AppFolderPopupContent = (props: { widget: IWidget }) => {
  const { widget } = props;
  const [isSaving, setIsSaving] = React.useState<boolean>(false);

  const { t } = useTranslation();

  const { removeWidget } = useWidgetStore();
  const { folderPath } = useAppStore();

  const { mutate: mutateApps } = useFetchApps();
  const { mutate: mutateGraphs } = useGraphs();

  const handleSetBaseDir = async (folderPath: string) => {
    try {
      await postLoadDir(folderPath.trim());
      await mutateApps();
      await mutateGraphs();
      toast.success(t("header.menuApp.loadAppSuccess"));
    } catch (error: unknown) {
      if (error instanceof Error) {
        toast.error(t("popup.default.errorOpenAppFolder"), {
          description: error.message,
        });
      } else {
        toast.error(t("popup.default.errorUnknown"));
      }
      console.error(error);
    }
  };

  const handleSave = async () => {
    setIsSaving(true);
    try {
      if (!folderPath.trim()) {
        toast.error(t("popup.default.errorFolderPathEmpty"));
        throw new Error("errorFolderPathEmpty");
      }
      await handleSetBaseDir(folderPath.trim());
      removeWidget(widget.widget_id);
    } catch (error: unknown) {
      if (error instanceof Error && error.message === "errorFolderPathEmpty") {
        toast.error(t("popup.default.errorFolderPathEmpty"));
      } else {
        toast.error(t("popup.default.errorUnknown"));
      }
      console.error(error);
    } finally {
      setIsSaving(false);
    }
  };

  return (
    <AppFileManager
      isSaveLoading={isSaving}
      onSave={handleSave}
      onCancel={() => removeWidget(widget.widget_id)}
    />
  );
};

export const LoadedAppsPopupTitle = () => {
  const { t } = useTranslation();
  return t("popup.apps.manager");
};

// eslint-disable-next-line @typescript-eslint/no-unused-vars
export const LoadedAppsPopupContent = (_props: { widget: IWidget }) => {
  return <AppsManagerWidget />;
};

export const AppRunPopupTitle = () => {
  const { t } = useTranslation();
  return t("popup.apps.run");
};

export const AppRunPopupContent = (props: { widget: IDefaultWidget }) => {
  const { widget } = props;
  const { base_dir: baseDir, scripts } = widget.metadata as IDefaultWidgetData;

  const { t } = useTranslation();

  const {
    removeWidget,
    appendWidget,
    appendBackstageWidget,
    // removeLogViewerHistory,
  } = useWidgetStore();

  const [selectedScript, setSelectedScript] = React.useState<
    string | undefined
  >(scripts?.[0] || undefined);
  const [runWithAgent, setRunWithAgent] = React.useState<boolean>(false);

  const handleRun = async () => {
    if (!baseDir || !selectedScript) {
      return;
    }

    removeWidget(widget.widget_id);

    const newAppStartWidgetId = EWidgetIdentifier.APP_RUN + Date.now();

    await addRecentRunApp({
      base_dir: baseDir || "",
      script_name: selectedScript || "",
      stdout_is_log: true,
      stderr_is_log: true,
      run_with_agent: runWithAgent,
    });

    appendBackstageWidget({
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
          base_dir: baseDir,
          name: selectedScript,
          stdout_is_log: true,
          stderr_is_log: true,
        },
      },
      // popup: {
      //   width: 0.5,
      //   height: 0.8,
      // },
      // actions: {
      //   onClose: () => {
      //     // Update(apps-manager):
      //     // keep the backstage widget after closing the popup
      //     // removeBackstageWidget(newAppStartWidgetId);
      //   },
      //   custom_actions: [
      //     {
      //       id: "app-start-log-clean",
      //       label: t("popup.logViewer.cleanLogs"),
      //       Icon: BrushCleaningIcon,
      //       onClick: () => {
      //         removeLogViewerHistory(newAppStartWidgetId);
      //       },
      //     },
      //   ],
      // },
    });

    if (runWithAgent) {
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

    widget?.actions?.onSubmit?.(widget.metadata);
  };

  if (!baseDir || !scripts || scripts.length === 0) {
    return null;
  }

  return (
    <div className="flex h-full w-full flex-col gap-2 p-2">
      <div className="flex flex-col gap-2">
        <Label htmlFor="runapp_base_dir">{t("popup.apps.baseDir")}</Label>
        <Input id="runapp_base_dir" type="text" value={baseDir} disabled />
      </div>
      <div className="flex flex-col gap-2">
        <Label htmlFor="runapp_script">{t("popup.apps.runScript")}</Label>
        <div>
          <Select value={selectedScript} onValueChange={setSelectedScript}>
            <SelectTrigger id="runapp_script">
              <SelectValue placeholder={t("popup.apps.selectScript")} />
            </SelectTrigger>
            <SelectContent>
              <SelectGroup>
                {scripts?.map((script) => (
                  <SelectItem key={script} value={script}>
                    {script}
                  </SelectItem>
                ))}
              </SelectGroup>
            </SelectContent>
          </Select>
        </div>
      </div>
      <Separator className="my-2" />
      <div className="mb-2 flex flex-col gap-2">
        <Label>{t("popup.apps.run_opts")}</Label>
        <div className="flex items-center space-x-2">
          <Checkbox
            id="runapp_with_rtc"
            checked={runWithAgent}
            onCheckedChange={() => setRunWithAgent(!runWithAgent)}
          />
          <Label htmlFor="runapp_with_rtc">
            {t("popup.apps.run_with_agent")}
          </Label>
        </div>
      </div>
      <div className="mt-auto flex justify-end gap-2">
        <Button
          variant="outline"
          onClick={() => removeWidget(widget.widget_id)}
        >
          {t("action.cancel")}
        </Button>
        <Button disabled={!selectedScript?.trim()} onClick={handleRun}>
          <PlayIcon className="size-4" />
          {t("action.run")}
        </Button>
      </div>
    </div>
  );
};

export const AppCreatePopupTitle = () => {
  const { t } = useTranslation();
  return t("popup.apps.create");
};

export const AppCreatePopupContent = (props: { widget: IWidget }) => {
  const { widget } = props;

  const { removeWidget } = useWidgetStore();

  const handleCreated = () => {
    removeWidget(widget.widget_id);
  };

  return (
    <AppTemplateWidget onCreated={handleCreated} className="w-full max-w-sm" />
  );
};
