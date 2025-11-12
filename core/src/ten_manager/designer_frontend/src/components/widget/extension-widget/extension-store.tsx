//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import type { TooltipContentProps } from "@radix-ui/react-tooltip";
import { AnimatePresence, motion } from "framer-motion";
import {
  BlocksIcon,
  BrushCleaningIcon,
  CheckLineIcon,
  DownloadIcon,
  FilterIcon,
  SearchIcon,
} from "lucide-react";
import * as React from "react";
import { useTranslation } from "react-i18next";
import AutoSizer from "react-virtualized-auto-sizer";
import { FixedSizeList as VirtualList } from "react-window";
import { toast } from "sonner";
import { useFetchAddons } from "@/api/services/addons";
import { postReloadApps, useFetchApps } from "@/api/services/apps";
import { useEnv } from "@/api/services/common";
import { useSearchTenCloudStorePackages } from "@/api/services/extension";
import { extractLocaleContentFromPkg } from "@/api/services/utils";
import { ExtensionPopupTitle } from "@/components/popup/default/extension";
import { LogViewerPopupTitle } from "@/components/popup/log-viewer";
import { SpinnerLoading } from "@/components/status/loading";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuGroup,
  DropdownMenuItem,
  DropdownMenuTrigger,
} from "@/components/ui/dropdown-menu";
import { Input } from "@/components/ui/input";
import { Tabs, TabsList, TabsTrigger } from "@/components/ui/tabs";
import {
  Tooltip,
  TooltipContent,
  TooltipProvider,
  TooltipTrigger,
} from "@/components/ui/tooltip";
// eslint-disable-next-line max-len
import { extensionListItemVariants } from "@/components/widget/extension-widget/common";
import { TEN_PATH_WS_BUILTIN_FUNCTION } from "@/constants";
import { getWSEndpointFromWindow } from "@/constants/utils";
import {
  CONTAINER_DEFAULT_ID,
  EXTENSION_WIDGET_ID,
  GROUP_EXTENSION_ID,
  GROUP_LOG_VIEWER_ID,
} from "@/constants/widgets";
import { calcAbbreviatedBaseDir, cn, compareVersions } from "@/lib/utils";
import { useAppStore, useWidgetStore } from "@/store";
import type { IApp, IExtensionAddon } from "@/types/apps";
import {
  EPackageSource,
  ETenPackageType,
  type IListTenCloudStorePackage,
  type ITenPackage,
  type ITenPackageLocal,
  TenPackageTypeMappings,
} from "@/types/extension";
import {
  ELogViewerScriptType,
  EWidgetCategory,
  EWidgetDisplayType,
} from "@/types/widgets";

const DEFAULT_SEARCH_FILTER = {
  field: "name",
  operator: "regex",
  value: ".*default.*",
};

const DEFAULT_SEARCH_OPTIONS = {
  scope: [
    "name",
    "version",
    "hash",
    "display_name",
    "tags",
    "dependencies",
    "downloadUrl",
    "type",
    "description",
  ].join(","),
};

export const ExtensionStoreWidget = (props: {
  className?: string;
  toolTipSide?: TooltipContentProps["side"];
}) => {
  const { className } = props;

  const [showFilters, setShowFilters] = React.useState(false);
  const [selectedType, setSelectedType] = React.useState<
    ETenPackageType | "all" | undefined
  >("all");

  const { extSearch, setExtSearch } = useWidgetStore();
  const deferredSearch = React.useDeferredValue(extSearch.trim());

  const { t } = useTranslation();
  const { setDefaultOsArch } = useAppStore();

  const searchFilter = React.useMemo(() => {
    if (!deferredSearch) {
      return DEFAULT_SEARCH_FILTER;
    }

    const regexValue = `.*${deferredSearch}.*`;

    return {
      or: [
        {
          field: "name",
          operator: "regex",
          value: regexValue,
        },
        {
          field: "display_name",
          operator: "regex",
          value: regexValue,
        },
      ],
    };
  }, [deferredSearch]);

  const searchPayload = React.useMemo(
    () => ({
      filter: searchFilter,
      options: DEFAULT_SEARCH_OPTIONS,
    }),
    [searchFilter]
  );

  const {
    data: searchedData,
    error: searchedDataError,
    isLoading: isSearchedDataLoading,
  } = useSearchTenCloudStorePackages(searchPayload);
  const { data: envData, error: envError, isLoading: isLoadingEnv } = useEnv();
  const {
    data: addons,
    error: addonError,
    isLoading: isFetchingAddons,
  } = useFetchAddons({});
  const {
    data: apps,
    error: appsError,
    isLoading: isLoadingApps,
  } = useFetchApps();

  const isLoading = React.useMemo(() => {
    return (
      isSearchedDataLoading || isLoadingEnv || isFetchingAddons || isLoadingApps
    );
  }, [isFetchingAddons, isLoadingEnv, isSearchedDataLoading, isLoadingApps]);
  const latestUniqueItems = React.useMemo(() => {
    const latestUniqueItemsMap = new Map<string, IListTenCloudStorePackage>();
    searchedData?.packages.forEach((pkg) => {
      const existing = latestUniqueItemsMap.get(pkg.name);
      if (!existing || compareVersions(pkg.version, existing.version) > 0) {
        latestUniqueItemsMap.set(pkg.name, pkg);
      }
    });
    return Array.from(latestUniqueItemsMap.values());
  }, [searchedData?.packages]);
  const typeCounts: Record<ETenPackageType, IListTenCloudStorePackage[]> =
    React.useMemo(() => {
      return latestUniqueItems.reduce(
        (acc, item) => {
          const type =
            (item.type as ETenPackageType) || ETenPackageType.Extension;
          acc[type].push(item);
          return acc;
        },
        {
          [ETenPackageType.Extension]: [],
          [ETenPackageType.App]: [],
          [ETenPackageType.AddonLoader]: [],
          [ETenPackageType.System]: [],
          [ETenPackageType.Protocol]: [],
        } as Record<ETenPackageType, IListTenCloudStorePackage[]>
      );
    }, [latestUniqueItems]);
  const displayedItems = React.useMemo(() => {
    if (selectedType === "all") {
      return latestUniqueItems;
    }
    return typeCounts[selectedType as ETenPackageType] || [];
  }, [latestUniqueItems, typeCounts, selectedType]);

  React.useEffect(() => {
    if (searchedDataError) {
      toast.error(
        t("extensionStore.searchError", {
          defaultValue:
            searchedDataError?.message || "Failed to search extensions",
        })
      );
    }
    if (envError) {
      toast.error(
        t("extensionStore.envError", {
          defaultValue: envError?.message || "Failed to fetch environment data",
        })
      );
    }
    if (addonError) {
      toast.error(
        t("extensionStore.addonError", {
          defaultValue: addonError?.message || "Failed to fetch addons",
        })
      );
    }
    if (appsError) {
      toast.error(
        t("extensionStore.appsError", {
          defaultValue: appsError?.message || "Failed to fetch apps",
        })
      );
    }
  }, [addonError, envError, searchedDataError, t, appsError]);

  React.useEffect(() => {
    if (envData?.os && envData?.arch) {
      setDefaultOsArch({ os: envData.os, arch: envData.arch });
    }
  }, [envData?.os, envData?.arch, setDefaultOsArch]);

  return (
    <div className="flex h-full w-full flex-col">
      {/* Search Bar */}
      <div className="relative">
        <SearchIcon
          className={cn(
            "-translate-y-1/2 absolute top-1/2 left-3",
            "h-4 w-4 transform text-muted-foreground"
          )}
        />
        <Input
          placeholder={t("extensionStore.searchPlaceholder", {
            defaultValue: "Search extensions...",
          })}
          value={extSearch}
          onChange={(e) => setExtSearch(e.target.value)}
          className="h-9 pr-10 pl-9"
        />
        <Button
          variant="ghost"
          size="sm"
          className={cn(
            "-translate-y-1/2 absolute top-1/2 right-1",
            "h-7 w-7 transform p-0"
          )}
          onClick={() => setShowFilters((prev) => !prev)}
        >
          <FilterIcon className="h-3 w-3" />
        </Button>
      </div>

      {/* Filter Tabs */}
      <AnimatePresence>
        {showFilters && (
          <motion.div
            initial={{ opacity: 0, height: 0 }}
            animate={{ opacity: 1, height: "auto" }}
            exit={{ opacity: 0, height: 0 }}
            transition={{ duration: 0.2 }}
            className="mt-3"
          >
            <Tabs
              value={selectedType}
              onValueChange={(value) =>
                setSelectedType(value as ETenPackageType)
              }
            >
              <TabsList className="grid h-8 w-full grid-cols-6">
                <TabsTrigger value={"all"} className="px-2 text-xs">
                  {t("extensionStore.packageType.all")} (
                  {Object.values(typeCounts).reduce((a, b) => a + b.length, 0)})
                </TabsTrigger>
                {Object.entries(TenPackageTypeMappings).map(([key, value]) => (
                  <TabsTrigger key={key} value={key} className="px-1 text-xs">
                    <value.icon className="size-3" />
                    {typeCounts[key as ETenPackageType].length}
                  </TabsTrigger>
                ))}
              </TabsList>
            </Tabs>
          </motion.div>
        )}
      </AnimatePresence>

      <div className="h-full flex-1">
        {/* <AnimatePresence mode="popLayout"> */}
        {isLoading && (
          <div
            className={cn(
              "flex h-full w-full items-center justify-center",
              className
            )}
          >
            <SpinnerLoading />
          </div>
        )}
        {!isLoading && (
          <div className="flex h-full flex-col gap-2 py-1">
            <AutoSizer key={`ExtensionStoreWidget-AutoSizer-${deferredSearch}`}>
              {({ width, height }: { width: number; height: number }) => {
                return (
                  <VirtualList
                    key={`ExtensionStoreWidget-VirtualList-${deferredSearch}`}
                    width={width}
                    height={height}
                    itemCount={displayedItems.length}
                    itemSize={96}
                  >
                    {(virtualProps) => (
                      <VirtualListItem
                        {...virtualProps}
                        key={`VirtualListItem-${
                          displayedItems[virtualProps.index]?.hash ||
                          virtualProps.index
                        }`}
                        items={displayedItems}
                        apps={apps?.app_info || []}
                        addons={addons || []}
                      />
                    )}
                  </VirtualList>
                );
              }}
            </AutoSizer>
          </div>
        )}
        {!isLoading && latestUniqueItems.length === 0 && (
          <motion.div
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            className={cn(
              "flex h-full flex-col items-center justify-center",
              "p-6 text-center",
              "my-auto"
            )}
          >
            <BlocksIcon className="mb-3 h-12 w-12 text-muted-foreground" />
            <h3 className="mb-1 font-medium text-foreground text-sm">
              {t("extensionStore.noExtensions", {
                defaultValue: "No extensions found",
              })}
            </h3>
            <p className="text-muted-foreground text-xs">
              {t("extensionStore.tryAdjusting", {
                defaultValue: "Try adjusting your search or filters",
              })}
            </p>
          </motion.div>
        )}
        {/* </AnimatePresence> */}
      </div>

      {/* Footer */}
      {/* <div className="">
        <div
          className={cn(
            "flex items-center justify-between text-ten-icontext-2 text-xs"
          )}
        >
          <span></span>
          <span>TEN Store</span>
        </div>
      </div> */}
    </div>
  );
};

const ExtensionItem = (props: {
  style?: React.CSSProperties;
  item: ITenPackage | ITenPackageLocal;
  variant?: ETenPackageType;
  className?: string;
  apps?: IApp[];
}) => {
  const {
    style,
    item,
    variant = ETenPackageType.Extension,
    className,
    apps: loadedApps,
  } = props;

  const { t, i18n } = useTranslation();
  const { appendWidget, removeBackstageWidget, removeLogViewerHistory } =
    useWidgetStore();

  const { mutate: mutateAddons } = useFetchAddons({});

  const isInstalled = item.isInstalled;

  const Icon = React.useMemo(() => {
    return TenPackageTypeMappings[variant].icon;
  }, [variant]);
  const prettyName = React.useMemo(() => {
    if (item._type === EPackageSource.Local) {
      return item.name;
    }
    return (
      extractLocaleContentFromPkg(item?.display_name, i18n.language) ||
      item.name
    );
  }, [i18n.language, item]);
  const prettyDescription = React.useMemo(() => {
    if (item._type === EPackageSource.Local) {
      return item.type || "";
    }
    return (
      extractLocaleContentFromPkg(item?.description, i18n.language) ||
      item.type ||
      ""
    );
  }, [i18n.language, item]);
  const tags = React.useMemo(() => {
    if (item._type === EPackageSource.Local) {
      return [];
    }
    return (item as ITenPackage).tags || [];
  }, [item]);

  const handleInstall =
    (baseDir: string, item: IListTenCloudStorePackage) =>
    (e: React.MouseEvent) => {
      e.preventDefault();
      e.stopPropagation();
      if (!baseDir || !item) {
        return;
      }
      const widgetId = `ext-install-${item.hash}`;
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
            base_dir: baseDir,
            pkg_type: item.type,
            pkg_name: item.name,
            pkg_version: item.version,
          },
          options: {
            disableSearch: true,
            title: t("popup.logViewer.appInstall"),
          },
          postActions: async () => {
            await mutateAddons();
            await postReloadApps(baseDir);
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
  const handleClick = () => {
    appendWidget({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: GROUP_EXTENSION_ID,
      widget_id: `${EXTENSION_WIDGET_ID}-${item.name}`,

      category: EWidgetCategory.Extension,
      display_type: EWidgetDisplayType.Popup,

      title: (
        <ExtensionPopupTitle
          name={
            extractLocaleContentFromPkg(
              (item as IListTenCloudStorePackage)?.display_name,
              i18n.language
            ) || item.name
          }
          type={item.type}
        />
      ),
      metadata: {
        name: item.name,
        type: item.type,
      },
      popup: {
        height: 0.8,
        width: 0.5,
        maxWidth: 600,
      },
    });
  };

  return (
    <motion.div
      layout
      // initial={{ opacity: 0, y: 20 }}
      animate={{ opacity: 1, y: 0 }}
      exit={{ opacity: 0, y: -20 }}
      transition={{ duration: 0.2 }}
      className={cn("h-24 w-full px-0.5 py-1", className)}
      style={style}
      onClick={handleClick}
    >
      <div
        className={cn(
          "flex h-full items-start gap-3 p-3",
          "cursor-pointer rounded-lg transition-[colors,box-shadow]",
          "inset-shadow-xs shadow-xs hover:shadow-md hover:ring-1",
          extensionListItemVariants({ text: variant })
        )}
      >
        {/* Content */}
        <div className="flex h-full min-w-0 flex-1 flex-col justify-between">
          <div className="mb-1 flex items-center gap-2">
            <h3 className="truncate font-medium text-foreground text-sm">
              {prettyName}
            </h3>
          </div>

          <TooltipProvider>
            <Tooltip>
              <TooltipTrigger asChild>
                <p
                  className={cn("mb-1 truncate text-muted-foreground text-xs")}
                >
                  {prettyDescription}
                </p>
              </TooltipTrigger>
              <TooltipContent className="max-w-xs">
                <p>{prettyDescription}</p>
              </TooltipContent>
            </Tooltip>
          </TooltipProvider>

          <div
            className={cn(
              "flex min-h-5 items-center gap-0.5 text-muted-foreground text-xs"
            )}
          >
            {tags.length > 0 &&
              tags.slice(0, 3).map((tag) => (
                <Badge
                  key={tag}
                  variant="outline"
                  className={cn(
                    "border-none",
                    extensionListItemVariants({ bg: variant, text: variant })
                  )}
                >
                  {tag}
                </Badge>
              ))}
          </div>
        </div>

        <div
          className={cn(
            "flex flex-col items-end justify-between gap-1",
            "h-full"
          )}
        >
          {/* Icon */}
          <div className="flex flex-shrink-0 items-center gap-1 py-0.5 text-xs">
            <Icon className={cn("inline size-3")} />
            <span>{t(TenPackageTypeMappings[variant].transKey)}</span>
          </div>
          {item._type === EPackageSource.Local && (
            <div className="h-5 px-1.5 py-0.5 text-ten-icontext-2 text-xs">
              {t("extensionStore.localAddonTip")}
            </div>
          )}
          {item._type === EPackageSource.Default && (
            <DropdownMenu>
              <DropdownMenuTrigger asChild>
                <Button
                  size="xs"
                  disabled={
                    isInstalled || (loadedApps && loadedApps.length === 0)
                  }
                  variant={isInstalled ? "outline" : "default"}
                  onClick={(e) => {
                    e.preventDefault();
                    e.stopPropagation();
                  }}
                >
                  {isInstalled ? (
                    <>
                      <CheckLineIcon />
                      <span className="sr-only">
                        {t("extensionStore.installed")}
                      </span>
                    </>
                  ) : (
                    <>
                      <DownloadIcon />
                      <span className="sr-only">
                        {t("extensionStore.install")}
                      </span>
                    </>
                  )}
                </Button>
              </DropdownMenuTrigger>
              <DropdownMenuContent className="w-fit" align="start">
                <DropdownMenuGroup>
                  {loadedApps?.map((app) => (
                    <DropdownMenuItem
                      key={`ext-details-app-${app.base_dir}`}
                      onClick={handleInstall(
                        app.base_dir,
                        item as IListTenCloudStorePackage
                      )}
                    >
                      {calcAbbreviatedBaseDir(app.base_dir)}
                    </DropdownMenuItem>
                  ))}
                </DropdownMenuGroup>
              </DropdownMenuContent>
            </DropdownMenu>
          )}
        </div>
      </div>
    </motion.div>
  );
};

const VirtualListItem = (props: {
  index: number;
  style: React.CSSProperties;
  items: IListTenCloudStorePackage[];
  apps?: IApp[];
  addons?: IExtensionAddon[];
  variant?: ETenPackageType;
}) => {
  const { items, apps, addons } = props;

  const item = items[props.index];

  return (
    <ExtensionItem
      style={props.style}
      key={`ExtensionItem-${item.hash}`}
      apps={apps}
      item={{
        ...item,
        _type: EPackageSource.Default,
        isInstalled: addons?.some((addon) => addon.name === item.name),
      }}
      variant={item.type}
    />
  );
};
