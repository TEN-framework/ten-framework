//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

/** biome-ignore-all lint/suspicious/noExplicitAny: <ignore> */

import { t } from "i18next";
import {
  BrushCleaningIcon,
  FolderMinusIcon,
  FolderPlusIcon,
  FolderSyncIcon,
  HardDriveDownloadIcon,
  LogsIcon,
  PlayIcon,
  RotateCcwIcon,
  SquareIcon,
} from "lucide-react";
import * as React from "react";
import { useTranslation } from "react-i18next";
import { toast } from "sonner";
import {
  postReloadApps,
  postUnloadApps,
  useFetchAppScripts,
  useFetchApps,
} from "@/api/services/apps";
import { useGraphs } from "@/api/services/graphs";
import {
  AppFolderPopupTitle,
  AppRunPopupTitle,
} from "@/components/popup/default/app";
import { LogViewerPopupTitle } from "@/components/popup/log-viewer";
import { SpinnerLoading } from "@/components/status/loading";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import {
  Table,
  TableBody,
  TableCaption,
  TableCell,
  TableFooter,
  TableHead,
  TableHeader,
  TableRow,
} from "@/components/ui/table";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import {
  Tooltip,
  TooltipContent,
  TooltipProvider,
  TooltipTrigger,
} from "@/components/ui/tooltip";
import { TEN_PATH_WS_BUILTIN_FUNCTION } from "@/constants";
import { getWSEndpointFromWindow } from "@/constants/utils";
import {
  APP_FOLDER_WIDGET_ID,
  APP_RUN_WIDGET_ID,
  CONTAINER_DEFAULT_ID,
  GROUP_LOG_VIEWER_ID,
} from "@/constants/widgets";
import { cn } from "@/lib/utils";
import { useDialogStore, useFlowStore, useWidgetStore } from "@/store";
import type { IApp } from "@/types/apps";
import {
  EDefaultWidgetType,
  ELogViewerScriptType,
  EWidgetCategory,
  EWidgetDisplayType,
  type ILogViewerWidget,
} from "@/types/widgets";

enum EAppTab {
  LOADED_APPS = "loaded-apps",
  RUNNING_SCRIPTS = "running-scripts",
}

const TabsContext = React.createContext<{
  selectedTab: EAppTab;
  handleChangeTab: (tab?: EAppTab) => void;
}>({
  selectedTab: EAppTab.LOADED_APPS,
  handleChangeTab: () => {},
});

export const AppsManagerWidget = (props: { className?: string }) => {
  const { className } = props;

  const [selectedTab, setSelectedTab] = React.useState<EAppTab>(
    EAppTab.LOADED_APPS
  );

  const handleChangeTab = (tab?: EAppTab) => {
    setSelectedTab(tab || EAppTab.LOADED_APPS);
  };

  return (
    <TabsContext.Provider value={{ selectedTab, handleChangeTab }}>
      <Tabs
        value={selectedTab}
        onValueChange={(val: string) => {
          setSelectedTab(val as EAppTab);
        }}
        className="h-full"
      >
        <TabsList>
          <TabsTrigger value={EAppTab.LOADED_APPS}>
            {t("popup.apps.loadedApps")}
          </TabsTrigger>
          <TabsTrigger value={EAppTab.RUNNING_SCRIPTS}>
            {t("popup.apps.runningScripts")}
          </TabsTrigger>
        </TabsList>
        <TabsContent value={EAppTab.LOADED_APPS}>
          <TabLoadedApps className={className} />
        </TabsContent>
        <TabsContent value={EAppTab.RUNNING_SCRIPTS}>
          <TabRunningScripts className={className} />
        </TabsContent>
      </Tabs>
    </TabsContext.Provider>
  );
};

const TabLoadedApps = (props: { className?: string }) => {
  const [isReloading, setIsReloading] = React.useState<boolean>(false);

  const { t } = useTranslation();
  const { data: loadedApps, isLoading, error, mutate } = useFetchApps();
  const { mutate: reloadGraphs } = useGraphs();
  const { appendWidget } = useWidgetStore();
  const { setNodesAndEdges } = useFlowStore();
  const { appendDialog, removeDialog } = useDialogStore();

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
        await reloadGraphs();
        removeDialog("reload-app");
      },
    });
  };

  const reloadApps = async (baseDir?: string) => {
    try {
      setIsReloading(true);
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
      await mutate();
      await reloadGraphs();
      setNodesAndEdges([], []);
      setIsReloading(false);
    }
  };

  const isLoadingMemo = React.useMemo(() => {
    return isReloading;
  }, [isReloading]);

  React.useEffect(() => {
    if (error) {
      toast.error(t("popup.apps.error"));
    }
  }, [error, t]);

  return (
    <TooltipProvider>
      <div
        className={cn(
          "flex h-full w-full flex-col gap-2 overflow-y-auto",
          props.className
        )}
      >
        <Table className="h-fit w-full border-none">
          <TableHeader>
            <TableRow className="border-none bg-muted/50 hover:bg-muted/50">
              <TableHead className="w-12 border-none text-center">
                {t("dataTable.no")}
              </TableHead>
              <TableHead className="border-none">
                {t("popup.apps.baseDir")}
              </TableHead>
              <TableHead className="border-none text-center">
                {t("popup.apps.status")}
              </TableHead>
              <TableHead className="border-none text-center">
                {t("dataTable.actions")}
              </TableHead>
            </TableRow>
          </TableHeader>
          <TableBody>
            {isLoading && (
              <TableRow className="border-none hover:bg-transparent">
                <TableCell colSpan={4} className="border-none">
                  <SpinnerLoading />
                </TableCell>
              </TableRow>
            )}
            {!isLoading && loadedApps?.app_info?.length === 0 && (
              <TableRow className="border-none hover:bg-transparent">
                <TableCell
                  colSpan={4}
                  className="border-none text-center text-muted-foreground"
                >
                  {t("popup.apps.emptyPlaceholder")}
                </TableCell>
              </TableRow>
            )}
            {!isLoading &&
              loadedApps?.app_info?.map((app, index) => (
                <AppRow key={`app-row-${app.base_dir}`} app={app} idx={index} />
              ))}
          </TableBody>
          <TableFooter className="border-none bg-transparent">
            <TableRow className="border-none hover:bg-transparent">
              <TableCell
                colSpan={4}
                className="space-x-2 border-none text-right"
              >
                <Button
                  variant="outline"
                  size="sm"
                  onClick={openAppFolderPopup}
                  disabled={isLoadingMemo}
                  className="gap-2 bg-transparent"
                >
                  <FolderPlusIcon className="h-4 w-4" />
                  {t("header.menuApp.loadApp")}
                </Button>
                <Button
                  variant="outline"
                  size="sm"
                  disabled={isLoadingMemo}
                  onClick={() => handleReloadApp()}
                  className="gap-2 bg-transparent"
                >
                  <FolderSyncIcon className="h-4 w-4" />
                  <span>{t("header.menuApp.reloadAllApps")}</span>
                </Button>
              </TableCell>
            </TableRow>
          </TableFooter>
          <TableCaption className="mt-auto select-none">
            {t("popup.apps.tableCaption")}
          </TableCaption>
        </Table>
      </div>
    </TooltipProvider>
  );
};

const AppRow = (props: { app: IApp; idx: number }) => {
  const { app, idx } = props;
  const [isActing, setIsActing] = React.useState<boolean>(false);

  const handleChangeTab = React.useContext(TabsContext).handleChangeTab;

  const { t } = useTranslation();
  const { mutate: mutateApps } = useFetchApps();
  const { mutate: reloadGraphs } = useGraphs();
  const {
    appendWidget,
    backstageWidgets, // Track running backstage widgets
    removeBackstageWidget,
    removeLogViewerHistory,
  } = useWidgetStore();
  const { setNodesAndEdges } = useFlowStore();
  const { appendDialog, removeDialog } = useDialogStore();

  const relatedBackstageWidges = React.useMemo(() => {
    return backstageWidgets.filter(
      (widget) =>
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        ((widget as ILogViewerWidget)?.metadata?.script as any)?.base_dir ===
        app.base_dir
    );
  }, [app.base_dir, backstageWidgets]);

  const handleStopApps = (baseDir: string) => {
    const backstageIds = backstageWidgets
      .filter(
        (widget) =>
          // eslint-disable-next-line @typescript-eslint/no-explicit-any
          ((widget as ILogViewerWidget)?.metadata?.script as any)?.base_dir ===
          baseDir
      )
      .map((widget) => widget.widget_id);
    backstageIds.forEach((id) => {
      removeBackstageWidget(id);
    });
  };

  const handleUnloadApp = async (baseDir: string) => {
    try {
      setIsActing(true);
      await postUnloadApps(baseDir);
      toast.success(t("header.menuApp.unloadAppSuccess"));
    } catch (error) {
      console.error(error);
      toast.error(
        t("header.menuApp.unloadAppFailed", {
          description:
            error instanceof Error ? error.message : t("popup.apps.error"),
        })
      );
    } finally {
      await mutateApps();
      await reloadGraphs();
      setIsActing(false);
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
        await reloadGraphs();
        removeDialog("reload-app");
      },
    });
  };

  const reloadApps = async (baseDir?: string) => {
    try {
      setIsActing(true);
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
      await reloadGraphs();
      setNodesAndEdges([], []);
      setIsActing(false);
    }
  };

  const handleAppInstallAll = (baseDir: string) => {
    const widgetId = `app-install-${Date.now()}`;
    appendWidget({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: GROUP_LOG_VIEWER_ID,
      widget_id: widgetId,

      category: EWidgetCategory.LogViewer,
      display_type: EWidgetDisplayType.Popup,

      title: <LogViewerPopupTitle />,
      metadata: {
        wsUrl: getWSEndpointFromWindow() + TEN_PATH_WS_BUILTIN_FUNCTION,
        scriptType: ELogViewerScriptType.INSTALL_ALL,
        script: {
          type: ELogViewerScriptType.INSTALL_ALL,
          base_dir: baseDir,
        },
        options: {
          disableSearch: true,
          title: t("popup.logViewer.appInstall"),
        },
        postActions: async () => {
          //   await reloadApps(baseDir);
        },
      },
      popup: {
        width: 0.5,
        height: 0.8,
      },
      actions: {
        onClose: async () => {
          removeBackstageWidget(widgetId);
          await reloadApps(baseDir);
        },
        custom_actions: [
          {
            id: "app-start-log-clean",
            label: t("popup.logViewer.cleanLogs"),
            Icon: BrushCleaningIcon,
            onClick: () => {
              removeLogViewerHistory(widgetId);
            },
          },
        ],
      },
    });
  };

  const handleRunApp = (baseDir: string, scripts: string[]) => {
    // Start frontstage widget (this can be closed without affecting backstage)
    appendWidget({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: APP_RUN_WIDGET_ID,
      widget_id: `${APP_RUN_WIDGET_ID}-${baseDir}`,

      category: EWidgetCategory.Default,
      display_type: EWidgetDisplayType.Popup,

      title: <AppRunPopupTitle />,
      metadata: {
        type: EDefaultWidgetType.AppRun,
        base_dir: baseDir,
        scripts: scripts,
      },
      popup: {
        width: 360,
      },
      actions: {
        onSubmit: () => {
          handleChangeTab(EAppTab.RUNNING_SCRIPTS);
        },
      },
    });
  };

  return (
    <TableRow className="border-none hover:bg-muted/30">
      <TableCell className={cn("border-none text-center", "font-mono text-sm")}>
        {(idx + 1).toString().padStart(2, "0")}
      </TableCell>
      <TableCell className="border-none">
        <span
          className={cn("rounded-md bg-muted p-1 px-2", "font-medium text-xs")}
        >
          {app.base_dir}
        </span>
      </TableCell>
      <TableCell className="border-none text-center">
        <Badge
          className="cursor-pointer"
          variant="secondary"
          onClick={() => {
            handleChangeTab(EAppTab.RUNNING_SCRIPTS);
          }}
        >
          {relatedBackstageWidges.length === 0 &&
            t("popup.apps.noRunningScripts")}
          {relatedBackstageWidges.length > 0 && (
            <span className="text-muted">
              {t("popup.apps.runningScriptsWithCount", {
                count: relatedBackstageWidges.length,
              })}
            </span>
          )}
        </Badge>
      </TableCell>
      <AppRowActions
        baseDir={app.base_dir}
        isLoading={isActing}
        handleUnloadApp={handleUnloadApp}
        handleReloadApp={handleReloadApp}
        handleAppInstallAll={handleAppInstallAll}
        handleRunApp={handleRunApp}
        handleStopApps={handleStopApps}
      />
    </TableRow>
  );
};

const AppRowActions = (props: {
  baseDir: string;
  isLoading?: boolean;
  handleUnloadApp: (baseDir: string) => void;
  handleReloadApp: (baseDir: string) => void;
  handleAppInstallAll: (baseDir: string) => void;
  handleRunApp: (baseDir: string, scripts: string[]) => void;
  handleStopApps: (baseDir: string) => void;
}) => {
  const {
    baseDir,
    isLoading,
    handleUnloadApp,
    handleReloadApp,
    handleAppInstallAll,
    handleRunApp,
    handleStopApps,
  } = props;

  const { t } = useTranslation();
  const {
    data: scripts,
    isLoading: isScriptsLoading,
    error: scriptsError,
  } = useFetchAppScripts(baseDir);
  const { backstageWidgets } = useWidgetStore();

  const relatedBackstageWidges = React.useMemo(() => {
    return backstageWidgets.filter(
      (widget) =>
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        ((widget as ILogViewerWidget)?.metadata?.script as any)?.base_dir ===
        baseDir
    );
  }, [baseDir, backstageWidgets]);

  React.useEffect(() => {
    if (scriptsError) {
      toast.error(t("popup.apps.error"), {
        description:
          scriptsError instanceof Error
            ? scriptsError.message
            : t("popup.apps.error"),
      });
    }
  }, [scriptsError, t]);

  const handleStopAll = () => {
    handleStopApps(baseDir);
  };

  const handleReload = () => {
    handleReloadApp(baseDir);
  };

  if (isLoading) {
    return (
      <TableCell colSpan={4} className="border-none text-center">
        <SpinnerLoading className="mx-auto size-4" />
      </TableCell>
    );
  }

  return (
    <TableCell>
      <div className="flex justify-center gap-1">
        {relatedBackstageWidges.length > 0 && (
          <Tooltip>
            <TooltipTrigger asChild>
              <Button
                variant="outline"
                size="sm"
                onClick={handleStopAll}
                className="h-8 w-8 p-0"
              >
                <SquareIcon className="h-3 w-3" />
              </Button>
            </TooltipTrigger>
            <TooltipContent>{t("action.stop")}</TooltipContent>
          </Tooltip>
        )}
        <Tooltip>
          <TooltipTrigger asChild>
            <Button
              variant="outline"
              size="sm"
              disabled={relatedBackstageWidges.length > 0}
              onClick={() => handleAppInstallAll(baseDir)}
              className="h-8 w-8 p-0"
            >
              <HardDriveDownloadIcon className="h-3 w-3" />
            </Button>
          </TooltipTrigger>
          <TooltipContent>{t("header.menuApp.installAll")}</TooltipContent>
        </Tooltip>

        <Tooltip>
          <TooltipTrigger asChild>
            <Button
              variant="outline"
              size="sm"
              disabled={isScriptsLoading || scripts?.length === 0}
              onClick={() => {
                handleRunApp(baseDir, scripts);
              }}
              className="h-8 w-8 p-0"
            >
              {isScriptsLoading ? (
                <SpinnerLoading className="size-4" />
              ) : (
                <PlayIcon className="size-4" />
              )}
            </Button>
          </TooltipTrigger>
          <TooltipContent>{t("header.menuApp.runApp")}</TooltipContent>
        </Tooltip>

        <Tooltip>
          <TooltipTrigger asChild>
            <Button
              variant="outline"
              size="sm"
              onClick={handleReload}
              className="h-8 w-8 p-0"
              disabled={relatedBackstageWidges.length > 0}
            >
              <RotateCcwIcon className="h-3 w-3" />
            </Button>
          </TooltipTrigger>
          <TooltipContent>{t("header.menuApp.reloadApp")}</TooltipContent>
        </Tooltip>

        <Tooltip>
          <TooltipTrigger asChild>
            <Button
              variant="outline"
              size="sm"
              className={cn(
                "h-8 w-8 bg-transparent p-0",
                "text-destructive hover:text-destructive"
              )}
              disabled={relatedBackstageWidges.length > 0}
              onClick={() => handleUnloadApp(baseDir)}
            >
              <FolderMinusIcon className="h-3 w-3" />
            </Button>
          </TooltipTrigger>
          <TooltipContent>{t("header.menuApp.unloadApp")}</TooltipContent>
        </Tooltip>
      </div>
    </TableCell>
  );
};

const TabRunningScripts = (props: { className?: string }) => {
  const { t } = useTranslation();
  const {
    appendWidget,
    backstageWidgets,
    removeBackstageWidget,
    removeLogViewerHistory,
  } = useWidgetStore();

  const logViewerWidgets = React.useMemo(() => {
    return backstageWidgets.filter(
      (widget) => widget.category === EWidgetCategory.LogViewer
    ) as ILogViewerWidget[];
  }, [backstageWidgets]);

  const handleStopScript = (widgetId: string) => {
    removeBackstageWidget(widgetId);
    toast.success(t("popup.apps.stopScriptSuccess"));
  };

  const handleOpenLogViewer = (widget: ILogViewerWidget) => {
    appendWidget({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: GROUP_LOG_VIEWER_ID,
      widget_id: widget.widget_id,

      category: EWidgetCategory.LogViewer,
      display_type: EWidgetDisplayType.Popup,

      title: <LogViewerPopupTitle />,
      metadata: widget.metadata,
      popup: {
        width: 0.5,
        height: 0.8,
      },
      actions: {
        onClose: () => {
          // Update(apps-manager):
          // keep the backstage widget after closing the popup
          // removeBackstageWidget(newAppStartWidgetId);
        },
        custom_actions: [
          {
            id: "app-start-log-clean",
            label: t("popup.logViewer.cleanLogs"),
            Icon: BrushCleaningIcon,
            onClick: () => {
              removeLogViewerHistory(widget.widget_id);
            },
          },
        ],
      },
    });
  };

  return (
    <TooltipProvider>
      <div
        className={cn(
          "flex h-full w-full flex-col gap-2 overflow-y-auto",
          props.className
        )}
      >
        <Table className="h-fit w-full border-none">
          <TableHeader>
            <TableRow className="border-none bg-muted/50 hover:bg-muted/50">
              <TableHead className="w-12 border-none text-center">
                {t("dataTable.no")}
              </TableHead>
              <TableHead className="border-none">
                {t("dataTable.name")}
              </TableHead>
              <TableHead className="border-none">
                {t("popup.apps.baseDir")}
              </TableHead>
              <TableHead className="border-none text-center">
                {t("dataTable.actions")}
              </TableHead>
            </TableRow>
          </TableHeader>
          <TableBody>
            {logViewerWidgets.length === 0 && (
              <TableRow className="border-none hover:bg-transparent">
                <TableCell
                  colSpan={5}
                  className="border-none text-center text-muted-foreground"
                >
                  {t("popup.apps.noRunningScripts")}
                </TableCell>
              </TableRow>
            )}
            {logViewerWidgets.map((widget, index) => (
              <TableRow
                key={widget.widget_id}
                className="border-none hover:bg-muted/30"
              >
                <TableCell
                  className={cn("border-none text-center", "font-mono text-sm")}
                >
                  {(index + 1).toString().padStart(2, "0")}
                </TableCell>
                <TableCell className="border-none">
                  <span
                    className={cn(
                      "rounded-md bg-muted p-1 px-2",
                      "font-medium text-xs"
                    )}
                  >
                    {(widget.metadata?.script as { name?: string })?.name ||
                      "N/A"}
                  </span>
                </TableCell>
                <TableCell className="border-none">
                  <span className="text-muted-foreground text-sm">
                    {(widget.metadata?.script as { base_dir?: string })
                      ?.base_dir || "N/A"}
                  </span>
                </TableCell>
                <TableCell className="border-none">
                  <div className="flex justify-center gap-1">
                    <Tooltip>
                      <TooltipTrigger asChild>
                        <Button
                          variant="outline"
                          size="sm"
                          onClick={() => handleOpenLogViewer(widget)}
                          className={cn("h-8 w-8 p-0")}
                        >
                          <LogsIcon className="h-3 w-3" />
                        </Button>
                      </TooltipTrigger>
                      <TooltipContent>
                        {t("action.launchLogViewer")}
                      </TooltipContent>
                    </Tooltip>
                    <Tooltip>
                      <TooltipTrigger asChild>
                        <Button
                          variant="outline"
                          size="sm"
                          onClick={() => handleStopScript(widget.widget_id)}
                          className={cn(
                            "h-8 w-8 p-0",
                            "text-destructive hover:text-destructive"
                          )}
                        >
                          <SquareIcon className="h-3 w-3" />
                        </Button>
                      </TooltipTrigger>
                      <TooltipContent>{t("action.stop")}</TooltipContent>
                    </Tooltip>
                  </div>
                </TableCell>
              </TableRow>
            ))}
          </TableBody>
        </Table>
        <TableCaption className="mt-auto select-none">
          {t("popup.apps.runningScriptsCaption")}
        </TableCaption>
      </div>
    </TooltipProvider>
  );
};
