//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as React from "react";
import { BlocksIcon, HardDriveDownloadIcon, CheckIcon } from "lucide-react";
import { useTranslation } from "react-i18next";

import { Button } from "@/components/ui/Button";
import { Badge, BadgeProps } from "@/components/ui/Badge";
import {
  Select,
  SelectContent,
  SelectGroup,
  SelectItem,
  SelectLabel,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/Select";
import { Separator } from "@/components/ui/Separator";
import { cn } from "@/lib/utils";
import { useAppStore, useWidgetStore } from "@/store";
import {
  EWidgetCategory,
  EWidgetDisplayType,
  ELogViewerScriptType,
} from "@/types/widgets";
import { TEN_PATH_WS_BUILTIN_FUNCTION } from "@/constants";
import { getWSEndpointFromWindow } from "@/constants/utils";

import type { IListTenCloudStorePackage } from "@/types/extension";
import { useListTenCloudStorePackages } from "@/api/services/extension";
import { postReloadApps } from "@/api/services/apps";
import { GROUP_LOG_VIEWER_ID } from "@/constants/widgets";
import { CONTAINER_DEFAULT_ID } from "@/constants/widgets";
import { LogViewerPopupTitle } from "@/components/Popup/LogViewer";

export const ExtensionTooltipContent = (props: {
  item: IListTenCloudStorePackage;
  versions: IListTenCloudStorePackage[];
}) => {
  const { item, versions } = props;

  const { t } = useTranslation();

  const supportsMemo = React.useMemo(() => {
    const result = new Map<string, { os: string; arch: string }>();
    for (const version of versions) {
      for (const support of version?.supports || []) {
        result.set(`${support.os} ${support.arch}`, {
          os: support.os,
          arch: support.arch,
        });
      }
    }
    return Array.from(result.values());
  }, [versions]);

  return (
    <div className="flex flex-col gap-1">
      <div className="font-semibold text-lg">
        {item.name}
        <Badge
          variant="secondary"
          className={cn(
            "text-xs px-2 py-0.5 whitespace-nowrap font-medium ",
            "ml-2"
          )}
        >
          {item.version}
        </Badge>
      </div>
      <div
        className={cn(
          "font-roboto-condensed",
          "text-xs text-gray-500 dark:text-gray-400"
        )}
      >
        {item.type}
      </div>
      <Separator />
      {supportsMemo.length > 0 && (
        <>
          <div className="text-gray-500 dark:text-gray-400">
            <div className="mb-1">{t("extensionStore.compatible")}</div>
            <ExtensionEleSupports name={item.name} supports={supportsMemo} />
          </div>
          <Separator />
        </>
      )}
      <div className="text-gray-500 dark:text-gray-400">
        <div className="mb-1">{t("extensionStore.dependencies")}</div>
        <ul className="flex flex-col gap-1 ml-2">
          {item.dependencies?.map((dependency) => (
            <li
              key={dependency.name}
              className="flex items-center w-full justify-between"
            >
              <span className="font-semibold">{dependency.name}</span>
              <span className="ml-1">{dependency.version}</span>
            </li>
          ))}
        </ul>
      </div>
    </div>
  );
};

export const ExtensionDetails = (props: {
  versions: IListTenCloudStorePackage[];
  name: string;
  className?: string;
  readOnly?: boolean;
}) => {
  const { versions, name, className, readOnly } = props;
  const [selectedVersion, setSelectedVersion] = React.useState<string>(
    versions[0].hash
  );

  const { addons, defaultOsArch } = useAppStore();
  const { mutate } = useListTenCloudStorePackages();

  const selectedVersionItemMemo = React.useMemo(() => {
    return versions.find((version) => version.hash === selectedVersion);
  }, [versions, selectedVersion]);
  const isInstalled = React.useMemo(() => {
    return addons.some((addon) => addon.name === name);
  }, [addons, name]);

  const { t } = useTranslation();
  const {
    appendWidgetIfNotExists,
    removeBackstageWidget,
    removeLogViewerHistory,
  } = useWidgetStore();
  const { currentWorkspace } = useAppStore();

  const osArchMemo = React.useMemo(() => {
    const result = new Map<string, IListTenCloudStorePackage[]>();
    for (const version of versions) {
      for (const support of version?.supports || []) {
        if (!result.has(`${support.os}/${support.arch}`)) {
          result.set(`${support.os}/${support.arch}`, []);
        }
        result.get(`${support.os}/${support.arch}`)?.push(version);
      }
    }
    if (result.size === 0) {
      result.set("default", versions);
    }
    return result;
  }, [versions]);

  const [osArch, setOsArch] = React.useState<string>(() => {
    if (defaultOsArch && defaultOsArch?.os && defaultOsArch?.arch) {
      if (osArchMemo.has(`${defaultOsArch.os}/${defaultOsArch.arch}`)) {
        return `${defaultOsArch.os}/${defaultOsArch.arch}`;
      }
    }
    return Array.from(osArchMemo.keys())[0];
  });

  const defaultVersionMemo = React.useMemo(() => {
    return osArchMemo.get(osArch)?.[0];
  }, [osArchMemo, osArch]);

  const handleOsArchChange = (value: string) => {
    setOsArch(value);
    setSelectedVersion(osArchMemo.get(value)?.[0]?.hash || versions[0].hash);
  };

  const handleSelectedVersionChange = (value: string) => {
    setSelectedVersion(value);
  };

  const handleInstall = () => {
    if (!currentWorkspace.app?.base_dir || !selectedVersionItemMemo) {
      return;
    }
    const widgetId = "ext-install-" + selectedVersionItemMemo.hash;
    appendWidgetIfNotExists({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: GROUP_LOG_VIEWER_ID,
      widget_id: widgetId,

      category: EWidgetCategory.LogViewer,
      display_type: EWidgetDisplayType.Popup,

      title: <LogViewerPopupTitle />,
      metadata: {
        wsUrl: getWSEndpointFromWindow() + TEN_PATH_WS_BUILTIN_FUNCTION,
        scriptType: ELogViewerScriptType.INSTALL,
        script: {
          type: ELogViewerScriptType.INSTALL,
          base_dir: currentWorkspace.app?.base_dir,
          pkg_type: selectedVersionItemMemo.type,
          pkg_name: selectedVersionItemMemo.name,
          pkg_version: selectedVersionItemMemo.version,
        },
        options: {
          disableSearch: true,
          title: t("popup.logViewer.appInstall"),
        },
        postActions: () => {
          mutate();
          if (currentWorkspace.app?.base_dir) {
            postReloadApps(currentWorkspace.app.base_dir);
          }
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

  return (
    <div
      className={cn(
        "w-full h-full flex flex-col p-4 gap-2",
        "overflow-y-auto",
        className
      )}
    >
      <div className="flex items-center gap-2">
        <BlocksIcon className="size-20" />
        <div className="flex flex-col gap-1">
          <div className="font-semibold text-lg">{name}</div>
          <div
            className={cn(
              "font-roboto-condensed",
              "text-gray-500 dark:text-gray-400"
            )}
          >
            {defaultVersionMemo?.type}
          </div>
          <div className="flex gap-2">
            {isInstalled ? (
              <Button
                variant="secondary"
                size="sm"
                disabled
                className={cn(
                  "cursor-pointer rounded-none",
                  "[&>svg]:size-3 px-2 py-0.5 h-fit",
                  "text-xs font-normal"
                )}
              >
                <CheckIcon className="size-3" />
                <span>{t("extensionStore.installed")}</span>
              </Button>
            ) : (
              <Button
                variant="secondary"
                size="sm"
                className={cn(
                  "cursor-pointer rounded-none",
                  "[&>svg]:size-3 px-2 py-0.5 h-fit",
                  "text-xs font-normal"
                )}
                disabled={readOnly}
                onClick={handleInstall}
              >
                <HardDriveDownloadIcon className="size-3" />
                <span>{t("extensionStore.install")}</span>
              </Button>
            )}
          </div>
        </div>
      </div>
      <Separator />
      {!osArchMemo.get("default")?.length && (
        <div className="flex items-center justify-between gap-2 w-full">
          <span className="font-semibold">{t("extensionStore.os-arch")}</span>
          <Select
            defaultValue={osArch}
            value={osArch}
            onValueChange={handleOsArchChange}
          >
            <SelectTrigger className="w-fit min-w-48">
              <SelectValue placeholder={t("extensionStore.selectOsArch")} />
            </SelectTrigger>
            <SelectContent>
              <SelectGroup>
                <SelectLabel>{t("extensionStore.os-arch")}</SelectLabel>
                {Array.from(osArchMemo.keys()).map((osArch) => (
                  <SelectItem key={osArch} value={osArch}>
                    {osArch === "default"
                      ? t("extensionStore.os-arch-default")
                      : osArch}
                  </SelectItem>
                ))}
              </SelectGroup>
            </SelectContent>
          </Select>
        </div>
      )}
      <div
        className="flex items-center justify-between gap-2 w-full"
        key={`${osArch}-selectedVersion`}
      >
        <span className="font-semibold">{t("extensionStore.version")}</span>
        <Select
          defaultValue={osArchMemo.get(osArch)?.[0]?.version}
          value={selectedVersion}
          onValueChange={handleSelectedVersionChange}
        >
          <SelectTrigger className="w-fit min-w-48">
            <SelectValue placeholder="Select a version" />
          </SelectTrigger>
          <SelectContent>
            <SelectGroup>
              <SelectLabel>{t("extensionStore.versionHistory")}</SelectLabel>
              {osArchMemo.get(osArch)?.map((version) => (
                <SelectItem key={version.hash} value={version.hash}>
                  {version.hash === defaultVersionMemo?.hash
                    ? `${version.version}(${t("extensionStore.versionLatest")})`
                    : version.version}
                </SelectItem>
              ))}
            </SelectGroup>
          </SelectContent>
        </Select>
      </div>
      <Separator />
      <div className="flex items-center justify-between gap-2 w-full">
        <span className="font-semibold">{t("extensionStore.hash")}</span>
        <ExtensionEleBadge className="max-w-20">
          {selectedVersionItemMemo?.hash?.slice(0, 8)}
        </ExtensionEleBadge>
      </div>
      <div className="flex items-start justify-between gap-2 w-full">
        <span className="font-semibold">
          {t("extensionStore.dependencies")}
        </span>
        <ul className="flex flex-col items-end gap-2">
          {selectedVersionItemMemo?.dependencies?.map((dependency) => (
            <li key={dependency.name}>
              <ExtensionEleBadge className="w-fit">
                <span className="font-semibold">{dependency.name}</span>
                <span className="ml-1">{dependency.version}</span>
              </ExtensionEleBadge>
            </li>
          ))}
        </ul>
      </div>
    </div>
  );
};

const ExtensionEleBadge = (props: BadgeProps) => {
  const { className, ...rest } = props;

  return (
    <Badge
      variant="secondary"
      className={cn(
        "text-xs px-2 py-0.5 whitespace-nowrap font-medium w-fit",
        className
      )}
      {...rest}
    />
  );
};

const ExtensionEleSupports = (props: {
  name: string;
  supports: IListTenCloudStorePackage["supports"];
  className?: string;
}) => {
  const { name, supports, className } = props;

  return (
    <ul className={cn("flex flex-wrap gap-1", className)}>
      {supports?.map((support) => (
        <ExtensionEleBadge key={`${name}-${support.os}-${support.arch}`}>
          {support.os}/{support.arch}
        </ExtensionEleBadge>
      ))}
    </ul>
  );
};
