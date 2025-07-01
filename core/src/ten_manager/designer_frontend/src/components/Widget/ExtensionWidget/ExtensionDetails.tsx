//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import {
  BlocksIcon,
  BrushCleaningIcon,
  CheckIcon,
  HardDriveDownloadIcon,
} from "lucide-react";
import * as React from "react";
import { useTranslation } from "react-i18next";
import Markdown from "react-markdown";
import remarkGfm from "remark-gfm";
import { useFetchAddons } from "@/api/services/addons";
import { postReloadApps } from "@/api/services/apps";
import { useListTenCloudStorePackages } from "@/api/services/extension";
import { extractLocaleContentFromPkg } from "@/api/services/utils";
import { LogViewerPopupTitle } from "@/components/Popup/LogViewer";
import { Badge, type BadgeProps } from "@/components/ui/Badge";
import { Button } from "@/components/ui/Button";
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
import { TEN_PATH_WS_BUILTIN_FUNCTION } from "@/constants";
import { getWSEndpointFromWindow } from "@/constants/utils";
import { CONTAINER_DEFAULT_ID, GROUP_LOG_VIEWER_ID } from "@/constants/widgets";
import { cn, compareVersions } from "@/lib/utils";
import { useAppStore, useWidgetStore } from "@/store";
import type { IListTenCloudStorePackage } from "@/types/extension";
import {
  ELogViewerScriptType,
  EWidgetCategory,
  EWidgetDisplayType,
} from "@/types/widgets";

export const ExtensionTooltipContent = (props: {
  item: IListTenCloudStorePackage;
  versions: IListTenCloudStorePackage[];
}) => {
  const { item, versions } = props;

  const { t, i18n } = useTranslation();

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

  const itemPrettyName = React.useMemo(() => {
    return (
      extractLocaleContentFromPkg(item?.display_name, i18n.language) ||
      item.name
    );
  }, [item, i18n.language]);

  return (
    <div className="flex flex-col gap-1">
      <div className="font-semibold text-lg">
        {itemPrettyName}
        <Badge
          variant="secondary"
          className={cn(
            "whitespace-nowrap px-2 py-0.5 font-medium text-xs ",
            "ml-2"
          )}
        >
          {item.version}
        </Badge>
      </div>
      <div
        className={cn(
          "font-roboto-condensed",
          "text-gray-500 text-xs dark:text-gray-400",
          "flex items-center justify-between gap-1"
        )}
      >
        <span>{item.type}</span>
        <span>{item.name}</span>
      </div>
      {item?.description?.locales?.[i18n.language] && (
        <>
          <Separator />
          <div className={cn("text-gray-500 dark:text-gray-400", "italic")}>
            <p>{item.description.locales[i18n.language].content}</p>
          </div>
        </>
      )}
      {supportsMemo.length > 0 && (
        <>
          <Separator />
          <div className="text-gray-500 dark:text-gray-400">
            <div className="mb-1">{t("extensionStore.compatible")}</div>
            <ExtensionEleSupports name={item.name} supports={supportsMemo} />
          </div>
        </>
      )}
      {item.dependencies?.length > 0 && (
        <>
          {" "}
          <Separator />
          <div className="text-gray-500 dark:text-gray-400">
            <div className="mb-1">{t("extensionStore.dependencies")}</div>
            <ul className="ml-2 flex flex-col gap-1">
              {item.dependencies?.map((dependency) => (
                <li
                  key={dependency.name}
                  className="flex w-full items-center justify-between"
                >
                  <span className="font-semibold">{dependency.name}</span>
                  <span className="ml-1">{dependency.version}</span>
                </li>
              ))}
            </ul>
          </div>
        </>
      )}
      {item.tags && item.tags.length > 0 && (
        <>
          <Separator />
          <div className="text-gray-500 dark:text-gray-400">
            <div className="mb-1">{t("extensionStore.tags")}</div>
            <ExtensionEleTags
              tags={item.tags}
              // className="flex flex-wrap gap-1 ml-2"
            />
          </div>
        </>
      )}
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

  const { currentWorkspace, defaultOsArch } = useAppStore();

  const { mutate: mutatePkgs } = useListTenCloudStorePackages();
  const { mutate: mutateAddons } = useFetchAddons({
    base_dir: currentWorkspace?.app?.base_dir,
  });
  const { data: addons } = useFetchAddons({
    base_dir: currentWorkspace.app?.base_dir || "",
  });

  const selectedVersionItemMemo = React.useMemo(() => {
    return versions.find((version) => version.hash === selectedVersion);
  }, [versions, selectedVersion]);
  const isInstalled = React.useMemo(() => {
    return addons.some((addon) => addon.name === name);
  }, [addons, name]);

  const { t, i18n } = useTranslation();
  const { appendWidget, removeBackstageWidget, removeLogViewerHistory } =
    useWidgetStore();

  const osArchMemo = React.useMemo(() => {
    const result = new Map<string, IListTenCloudStorePackage[]>();
    const sortedVersions = [...versions].sort((a, b) =>
      compareVersions(b.version, a.version)
    );
    for (const version of sortedVersions) {
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
    if (defaultOsArch?.os && defaultOsArch?.arch) {
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
    appendWidget({
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
          mutatePkgs();
          mutateAddons();
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

  const [prettyName, prettyDesc, prettyReadme] = React.useMemo(() => {
    const displayName =
      extractLocaleContentFromPkg(
        defaultVersionMemo?.display_name,
        i18n.language
      ) || defaultVersionMemo?.name;
    const description = extractLocaleContentFromPkg(
      defaultVersionMemo?.description,
      i18n.language
    );
    const readme = extractLocaleContentFromPkg(
      defaultVersionMemo?.readme,
      i18n.language
    );
    return [
      displayName || defaultVersionMemo?.name,
      description || "",
      readme || "",
    ];
  }, [defaultVersionMemo, i18n.language]);

  return (
    <div
      className={cn(
        "flex h-full w-full flex-col gap-2 p-4",
        "overflow-y-auto",
        className
      )}
    >
      <div className="flex items-center gap-2">
        <BlocksIcon className="mb-auto size-20" />
        <div className="flex flex-col gap-1">
          <div className="font-semibold text-lg">{prettyName}</div>
          {prettyDesc && (
            <div
              className={cn(
                "text-gray-500 dark:text-gray-400",
                "italic",
                "max-w-md"
              )}
            >
              {prettyDesc}
            </div>
          )}
          <div className="flex gap-2">
            {isInstalled ? (
              <Button
                variant="secondary"
                size="sm"
                disabled
                className={cn(
                  "cursor-pointer rounded-none",
                  "h-fit px-2 py-0.5 [&>svg]:size-3",
                  "font-normal text-xs"
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
                  "h-fit px-2 py-0.5 [&>svg]:size-3",
                  "font-normal text-xs"
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
      <div className={cn({ "flex gap-2": prettyReadme })}>
        {prettyReadme && (
          <div className={cn("prose-sm dark:prose-invert", "max-w-md ")}>
            <Markdown remarkPlugins={[remarkGfm]}>{prettyReadme}</Markdown>
          </div>
        )}
        <div className="flex flex-col gap-2">
          <TwoColsLayout label={t("extensionStore.filter.type")}>
            <span
              className={cn(
                "font-roboto-condensed",
                "text-gray-500 dark:text-gray-400"
              )}
            >
              {defaultVersionMemo?.type}
            </span>
          </TwoColsLayout>
          <TwoColsLayout label={t("extensionStore.identifier")}>
            <span
              className={cn(
                "font-roboto-condensed",
                "text-gray-500 dark:text-gray-400"
              )}
            >
              {name}
            </span>
          </TwoColsLayout>
          <Separator />
          {!osArchMemo.get("default")?.length && (
            <TwoColsLayout label={t("extensionStore.os-arch")}>
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
            </TwoColsLayout>
          )}
          <TwoColsLayout
            label={t("extensionStore.version")}
            key={`${osArch}-selectedVersion`}
          >
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
                  <SelectLabel>
                    {t("extensionStore.versionHistory")}
                  </SelectLabel>
                  {osArchMemo.get(osArch)?.map((version) => (
                    <SelectItem key={version.hash} value={version.hash}>
                      {version.hash === defaultVersionMemo?.hash
                        ? // eslint-disable-next-line max-len
                          `${version.version}(${t("extensionStore.versionLatest")})`
                        : version.version}
                    </SelectItem>
                  ))}
                </SelectGroup>
              </SelectContent>
            </Select>
          </TwoColsLayout>
          <Separator />
          <TwoColsLayout label={t("extensionStore.hash")}>
            <ExtensionEleBadge className="max-w-20">
              {selectedVersionItemMemo?.hash?.slice(0, 8)}
            </ExtensionEleBadge>
          </TwoColsLayout>
          <TwoColsLayout label={t("extensionStore.dependencies")}>
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
          </TwoColsLayout>
          {selectedVersionItemMemo?.tags &&
            selectedVersionItemMemo.tags.length > 0 && (
              <>
                <Separator />
                <div className="font-semibold">{t("extensionStore.tags")}</div>
                <ExtensionEleTags tags={selectedVersionItemMemo.tags} />
              </>
            )}
        </div>
      </div>
    </div>
  );
};

const TwoColsLayout = (props: {
  children?: React.ReactNode;
  label?: string | React.ReactNode;
  className?: string;
}) => {
  const { children, label, className } = props;
  return (
    <div
      className={cn("flex w-full items-start justify-between gap-2", className)}
    >
      {typeof label === "string" ? (
        <span className="font-semibold">{label}</span>
      ) : (
        label
      )}
      {children}
    </div>
  );
};

const ExtensionEleBadge = (props: BadgeProps) => {
  const { className, ...rest } = props;

  return (
    <Badge
      variant="secondary"
      className={cn(
        "w-fit whitespace-nowrap px-2 py-0.5 font-medium text-xs",
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

const ExtensionEleTags = (props: {
  tags: IListTenCloudStorePackage["tags"];
  maxItemsPerRow?: number;
  className?: string;
}) => {
  const { tags, maxItemsPerRow = 6, className } = props;

  const tagsMemo = React.useMemo(() => {
    if (!tags || tags.length === 0) {
      return { rows: [[]] as string[][], tagsSet: new Set<string>() };
    }
    return tags?.reduce(
      (acc, tag) => {
        const isTagExist = acc.tagsSet.has(tag);
        if (isTagExist) {
          return acc;
        }
        acc.tagsSet.add(tag);
        const lastRow = acc.rows[acc.rows.length - 1];
        if (lastRow.length < maxItemsPerRow) {
          lastRow.push(tag);
        } else {
          acc.rows.push([tag]);
        }
        return acc;
      },
      { rows: [[]] as string[][], tagsSet: new Set<string>() }
    );
  }, [tags, maxItemsPerRow]);

  return (
    <ul className={cn("flex flex-col gap-1", className)}>
      {tagsMemo.rows.map((row, rowIndex) => (
        <li
          key={rowIndex}
          className={cn("flex items-center justify-start gap-1", {
            ["justify-between"]: row.length === maxItemsPerRow,
          })}
        >
          {row.map((tag) => (
            <ExtensionEleBadge key={tag}>{tag}</ExtensionEleBadge>
          ))}
        </li>
      ))}
    </ul>
  );
};
