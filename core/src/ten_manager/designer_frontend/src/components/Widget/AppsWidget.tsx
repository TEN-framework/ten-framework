//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
/* eslint-disable react-hooks/exhaustive-deps */
import * as React from "react";
import { useTranslation } from "react-i18next";
import { toast } from "sonner";
import {
  FolderMinusIcon,
  FolderPlusIcon,
  FolderSyncIcon,
  HardDriveDownloadIcon,
  PlayIcon,
  FolderIcon,
} from "lucide-react";
import { zodResolver } from "@hookform/resolvers/zod";
import { useForm } from "react-hook-form";
import { z } from "zod";

import {
  Table,
  TableBody,
  TableCaption,
  TableCell,
  TableFooter,
  TableHead,
  TableHeader,
  TableRow,
} from "@/components/ui/Table";
import {
  Tooltip,
  TooltipContent,
  TooltipProvider,
  TooltipTrigger,
} from "@/components/ui/Tooltip";
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
  DialogTrigger,
} from "@/components/ui/Dialog";
import { Button } from "@/components/ui/Button";
import {
  Form,
  FormControl,
  FormDescription,
  FormField,
  FormItem,
  FormLabel,
  FormMessage,
} from "@/components/ui/Form";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/Select";
import { Input } from "@/components/ui/Input";
import { cn } from "@/lib/utils";
import {
  useApps,
  useAppScripts,
  retrieveTemplatePkgs,
  postReloadApps,
  postUnloadApps,
  postCreateApp,
} from "@/api/services/apps";
import { SpinnerLoading } from "@/components/Status/Loading";
import {
  useWidgetStore,
  useFlowStore,
  useAppStore,
  useDialogStore,
} from "@/store";
import {
  EDefaultWidgetType,
  EWidgetDisplayType,
  EWidgetCategory,
  ELogViewerScriptType,
} from "@/types/widgets";
import {
  APP_FOLDER_WIDGET_ID,
  APP_RUN_WIDGET_ID,
  CONTAINER_DEFAULT_ID,
  GROUP_LOG_VIEWER_ID,
} from "@/constants/widgets";
import { TEN_PATH_WS_BUILTIN_FUNCTION } from "@/constants";
import { getWSEndpointFromWindow } from "@/constants/utils";
import {
  ETemplateLanguage,
  ETemplateType,
  TemplatePkgsReqSchema,
  AppCreateReqSchema,
} from "@/types/apps";
import { AppFileManager } from "@/components/FileManager/AppFolder";
import {
  AppFolderPopupTitle,
  AppRunPopupTitle,
} from "@/components/Popup/Default/App";
import { LogViewerPopupTitle } from "../Popup/LogViewer";

export const AppsManagerWidget = (props: { className?: string }) => {
  const [isUnloading, setIsUnloading] = React.useState<boolean>(false);
  const [isReloading, setIsReloading] = React.useState<boolean>(false);

  const { t } = useTranslation();
  const { data: loadedApps, isLoading, error, mutate } = useApps();
  const {
    appendWidgetIfNotExists,
    removeBackstageWidget,
    removeLogViewerHistory,
  } = useWidgetStore();
  const { setNodesAndEdges } = useFlowStore();
  const { currentWorkspace, updateCurrentWorkspace } = useAppStore();
  const { appendDialog, removeDialog } = useDialogStore();

  const openAppFolderPopup = () => {
    appendWidgetIfNotExists({
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

  const handleUnloadApp = async (baseDir: string) => {
    try {
      setIsUnloading(true);
      await postUnloadApps(baseDir);
      if (currentWorkspace.app?.base_dir === baseDir) {
        setNodesAndEdges([], []);
        updateCurrentWorkspace({
          app: null,
          graph: null,
        });
      }
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
      mutate();
      setIsUnloading(false);
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
      postConfirm: async () => {
        setNodesAndEdges([], []);
        updateCurrentWorkspace({
          graph: null,
        });
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
      mutate();
      setIsReloading(false);
    }
  };

  const handleAppInstallAll = (baseDir: string) => {
    const widgetId = "app-install-" + Date.now();
    appendWidgetIfNotExists({
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
        postActions: () => {
          postReloadApps(baseDir);
        },
      },
      popup: {
        width: 0.5,
        height: 0.8,
      },
      actions: {
        onClose: () => {
          removeBackstageWidget(widgetId);
          removeLogViewerHistory(widgetId);
        },
      },
    });
  };

  const handleRunApp = (baseDir: string, scripts: string[]) => {
    appendWidgetIfNotExists({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: APP_RUN_WIDGET_ID,
      widget_id: APP_RUN_WIDGET_ID + "-" + baseDir,

      category: EWidgetCategory.Default,
      display_type: EWidgetDisplayType.Popup,

      title: <AppRunPopupTitle />,
      metadata: {
        type: EDefaultWidgetType.AppRun,
        base_dir: baseDir,
        scripts: scripts,
      },
    });
  };

  const isLoadingMemo = React.useMemo(() => {
    return isUnloading || isReloading || isLoading;
  }, [isUnloading, isReloading, isLoading]);

  React.useEffect(() => {
    if (error) {
      toast.error(t("popup.apps.error"));
    }
  }, [error]);

  return (
    <div className={cn("flex flex-col gap-2 w-full h-full", props.className)}>
      <Table>
        <TableCaption className="select-none">
          {t("popup.apps.tableCaption")}
        </TableCaption>
        <TableHeader>
          <TableRow>
            <TableHead className="w-14">{t("dataTable.no")}</TableHead>
            <TableHead>{t("dataTable.name")}</TableHead>
            <TableHead className="text-right">
              {t("dataTable.actions")}
            </TableHead>
          </TableRow>
        </TableHeader>
        <TableBody>
          {isLoading ? (
            <TableRow>
              <TableCell colSpan={3}>
                <SpinnerLoading />
              </TableCell>
            </TableRow>
          ) : (
            loadedApps?.app_info?.map((app, index) => (
              <TableRow key={app.base_dir}>
                <TableCell className="font-medium">{index + 1}</TableCell>
                <TableCell>
                  <span className={cn("text-xs rounded-md p-1 bg-muted px-2")}>
                    {app.base_dir}
                  </span>
                </TableCell>
                <AppRowActions
                  baseDir={app.base_dir}
                  isLoading={isLoadingMemo}
                  handleUnloadApp={handleUnloadApp}
                  handleReloadApp={handleReloadApp}
                  handleAppInstallAll={handleAppInstallAll}
                  handleRunApp={handleRunApp}
                />
              </TableRow>
            ))
          )}
        </TableBody>
        <TableFooter className="bg-transparent">
          <TableRow>
            <TableCell colSpan={3} className="text-right space-x-2">
              <Button
                variant="outline"
                size="sm"
                onClick={openAppFolderPopup}
                disabled={isLoadingMemo}
              >
                <FolderPlusIcon className="w-4 h-4" />
                {t("header.menuApp.loadApp")}
              </Button>
              <Button
                variant="outline"
                size="sm"
                disabled={isLoadingMemo}
                onClick={() => handleReloadApp()}
              >
                <FolderSyncIcon className="w-4 h-4" />
                <span>{t("header.menuApp.reloadAllApps")}</span>
              </Button>
            </TableCell>
          </TableRow>
        </TableFooter>
      </Table>
    </div>
  );
};

const AppRowActions = (props: {
  baseDir: string;
  isLoading?: boolean;
  handleUnloadApp: (baseDir: string) => void;
  handleReloadApp: (baseDir: string) => void;
  handleAppInstallAll: (baseDir: string) => void;
  handleRunApp: (baseDir: string, scripts: string[]) => void;
}) => {
  const {
    baseDir,
    isLoading,
    handleUnloadApp,
    handleReloadApp,
    handleAppInstallAll,
    handleRunApp,
  } = props;

  const { t } = useTranslation();
  const {
    data: scripts,
    isLoading: isScriptsLoading,
    error: scriptsError,
  } = useAppScripts(baseDir);

  React.useEffect(() => {
    if (scriptsError) {
      toast.error(t("popup.apps.error"), {
        description:
          scriptsError instanceof Error
            ? scriptsError.message
            : t("popup.apps.error"),
      });
    }
  }, [scriptsError]);

  return (
    <TableCell className="text-right flex items-center gap-2">
      <TooltipProvider>
        <Tooltip>
          <TooltipTrigger asChild>
            <Button
              variant="outline"
              size="icon"
              disabled={isLoading}
              onClick={() => handleUnloadApp(baseDir)}
            >
              <FolderMinusIcon className="w-4 h-4" />
              <span className="sr-only">{t("header.menuApp.unloadApp")}</span>
            </Button>
          </TooltipTrigger>
          <TooltipContent>
            <p>{t("header.menuApp.unloadApp")}</p>
          </TooltipContent>
        </Tooltip>
      </TooltipProvider>

      <TooltipProvider>
        <Tooltip>
          <TooltipTrigger asChild>
            <Button
              variant="outline"
              size="icon"
              disabled={isLoading}
              onClick={() => handleReloadApp(baseDir)}
            >
              <FolderSyncIcon className="w-4 h-4" />
              <span className="sr-only">{t("header.menuApp.reloadApp")}</span>
            </Button>
          </TooltipTrigger>
          <TooltipContent>
            <p>{t("header.menuApp.reloadApp")}</p>
          </TooltipContent>
        </Tooltip>
      </TooltipProvider>

      <TooltipProvider>
        <Tooltip>
          <TooltipTrigger asChild>
            <Button
              variant="outline"
              size="icon"
              disabled={isLoading}
              onClick={() => handleAppInstallAll(baseDir)}
            >
              <HardDriveDownloadIcon className="w-4 h-4" />
              <span className="sr-only">{t("header.menuApp.installAll")}</span>
            </Button>
          </TooltipTrigger>
          <TooltipContent>
            <p>{t("header.menuApp.installAll")}</p>
          </TooltipContent>
        </Tooltip>
      </TooltipProvider>

      <TooltipProvider>
        <Tooltip>
          <TooltipTrigger asChild>
            <Button
              variant="outline"
              size="icon"
              disabled={isLoading || isScriptsLoading || scripts?.length === 0}
              onClick={() => {
                handleRunApp(baseDir, scripts);
              }}
            >
              {isScriptsLoading ? (
                <SpinnerLoading className="size-4" />
              ) : (
                <PlayIcon className="size-4" />
              )}
            </Button>
          </TooltipTrigger>
          <TooltipContent>
            <p>{t("header.menuApp.runApp")}</p>
          </TooltipContent>
        </Tooltip>
      </TooltipProvider>
    </TableCell>
  );
};

export const AppTemplateWidget = (props: {
  className?: string;
  onCreated?: (baseDir: string) => void;
}) => {
  const { className, onCreated } = props;
  const [templatePkgs, setTemplatePkgs] = React.useState<
    Record<string, string[]>
  >({});
  const [showAppFolder, setShowAppFolder] = React.useState<boolean>(false);
  const [isCreating, setIsCreating] = React.useState<boolean>(false);

  const { t } = useTranslation();
  const { mutate: mutateApps } = useApps();

  const formSchema = z.object({
    ...TemplatePkgsReqSchema.shape,
    ...AppCreateReqSchema.shape,
  });

  const form = useForm<z.infer<typeof formSchema>>({
    resolver: zodResolver(formSchema),
    defaultValues: {
      pkg_type: ETemplateType.APP,
      language: ETemplateLanguage.CPP,
      base_dir: undefined,
      app_name: undefined,
      template_name: undefined,
    },
  });

  const onSubmit = async (values: z.infer<typeof formSchema>) => {
    try {
      setIsCreating(true);
      const res = await postCreateApp(
        values.base_dir,
        values.template_name,
        values.app_name
      );
      toast.success(t("popup.apps.createAppSuccess"), {
        description: res?.app_path || values.base_dir,
      });
      mutateApps();
      onCreated?.(res?.app_path || values.base_dir);
    } catch (error) {
      console.error(error);
      toast.error(t("popup.apps.createAppFailed"), {
        description:
          error instanceof Error
            ? error.message
            : t("popup.apps.createAppFailed"),
      });
    } finally {
      setIsCreating(false);
    }
  };

  React.useEffect(() => {
    const fetchTemplatePkgs = async () => {
      const key = `${form.watch("pkg_type")}-${form.watch("language")}`;
      const existingPkgs = templatePkgs[key];
      if (!existingPkgs || existingPkgs.length === 0) {
        const pkgs = await retrieveTemplatePkgs(
          form.watch("pkg_type"),
          form.watch("language")
        );
        setTemplatePkgs((prev) => ({ ...prev, [key]: pkgs.template_name }));
      }
    };
    fetchTemplatePkgs();
  }, [form.watch("pkg_type"), form.watch("language")]);

  return (
    <Form {...form}>
      <form
        onSubmit={form.handleSubmit(onSubmit)}
        className={cn(
          "space-y-4 px-2",
          "max-h-[calc(90dvh-10rem)] overflow-y-auto",
          className
        )}
      >
        <FormField
          control={form.control}
          name="pkg_type"
          render={({ field }) => (
            <FormItem>
              <FormLabel>{t("popup.apps.templateType")}</FormLabel>
              <Select onValueChange={field.onChange} defaultValue={field.value}>
                <FormControl>
                  <SelectTrigger className="w-full max-w-sm">
                    <SelectValue placeholder={t("popup.apps.templateType")} />
                  </SelectTrigger>
                </FormControl>
                <SelectContent className="w-full max-w-sm">
                  {Object.values(ETemplateType).map((type) => (
                    <SelectItem key={type} value={type}>
                      {type}
                    </SelectItem>
                  ))}
                </SelectContent>
              </Select>
              <FormDescription>
                {t("popup.apps.templateTypeDescription")}
              </FormDescription>
              <FormMessage />
            </FormItem>
          )}
        />
        <FormField
          control={form.control}
          name="language"
          render={({ field }) => (
            <FormItem>
              <FormLabel>{t("popup.apps.templateLanguage")}</FormLabel>
              <Select onValueChange={field.onChange} defaultValue={field.value}>
                <FormControl>
                  <SelectTrigger className="w-full max-w-sm">
                    <SelectValue
                      placeholder={t("popup.apps.templateLanguage")}
                    />
                  </SelectTrigger>
                </FormControl>
                <SelectContent className="w-full max-w-sm">
                  {Object.values(ETemplateLanguage).map((language) => (
                    <SelectItem key={language} value={language}>
                      {language}
                    </SelectItem>
                  ))}
                </SelectContent>
              </Select>
              <FormDescription>
                {t("popup.apps.templateLanguageDescription")}
              </FormDescription>
              <FormMessage />
            </FormItem>
          )}
        />
        <FormField
          control={form.control}
          name="template_name"
          render={({ field }) => (
            <FormItem>
              <FormLabel>{t("popup.apps.templateName")}</FormLabel>
              <Select
                onValueChange={field.onChange}
                defaultValue={field.value}
                disabled={
                  !templatePkgs[
                    `${form.watch("pkg_type")}-${form.watch("language")}`
                  ]
                }
              >
                <FormControl>
                  <SelectTrigger className="w-full max-w-sm">
                    <SelectValue
                      placeholder={t("popup.apps.templateLanguage")}
                    />
                  </SelectTrigger>
                </FormControl>
                <SelectContent className="w-full max-w-sm">
                  {templatePkgs[
                    `${form.watch("pkg_type")}-${form.watch("language")}`
                  ]?.map((pkg) => (
                    <SelectItem key={pkg} value={pkg}>
                      {pkg}
                    </SelectItem>
                  ))}
                </SelectContent>
              </Select>
              <FormDescription>
                {t("popup.apps.templateLanguageDescription")}
              </FormDescription>
              <FormMessage />
            </FormItem>
          )}
        />
        <FormField
          control={form.control}
          name="app_name"
          render={({ field }) => (
            <FormItem>
              <FormLabel>{t("popup.apps.appName")}</FormLabel>
              <Input {...field} />
              <FormMessage />
            </FormItem>
          )}
        />
        <FormField
          control={form.control}
          name="base_dir"
          render={({ field }) => (
            <FormItem>
              <FormLabel>{t("popup.apps.baseDir")}</FormLabel>
              <Dialog open={showAppFolder} onOpenChange={setShowAppFolder}>
                <DialogTrigger asChild>
                  <Button
                    variant="outline"
                    size="sm"
                    className="w-full max-w-sm"
                  >
                    <span className="text-ellipsis overflow-hidden">
                      {field.value || t("action.chooseBaseDir")}
                    </span>
                    <FolderIcon className="w-4 h-4" />
                  </Button>
                </DialogTrigger>
                <DialogContent className="h-fit w-fit max-w-screen">
                  <DialogHeader>
                    <DialogTitle>{t("popup.apps.baseDir")}</DialogTitle>
                    <DialogDescription>
                      {t("popup.apps.baseDirDescription")}
                    </DialogDescription>
                  </DialogHeader>
                  <AppFileManager
                    className="h-[400px] w-[600px]"
                    onSave={(folderPath) => {
                      field.onChange(folderPath);
                      setShowAppFolder(false);
                    }}
                  />
                </DialogContent>
              </Dialog>
              <FormMessage />
            </FormItem>
          )}
        />
        <Button
          type="submit"
          size="sm"
          disabled={!form.formState.isValid || isCreating}
          className="w-full"
        >
          {isCreating && <SpinnerLoading className="size-4" />}
          {t("action.create")}
        </Button>
      </form>
    </Form>
  );
};
