//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import type { TooltipContentProps } from "@radix-ui/react-tooltip";
import { cva, type VariantProps } from "class-variance-authority";
import { AnimatePresence, motion } from "framer-motion";
import {
  BlocksIcon,
  CheckLineIcon,
  ChromeIcon,
  DownloadIcon,
  FilterIcon,
  SearchIcon,
  SettingsIcon,
  StarIcon,
} from "lucide-react";
import * as React from "react";
import { useTranslation } from "react-i18next";
import { toast } from "sonner";
import type z from "zod";
import { useFetchAddons } from "@/api/services/addons";
import { useEnv } from "@/api/services/common";
import {
  useListTenCloudStorePackages,
  useSearchTenCloudStorePackages,
} from "@/api/services/extension";
import { extractLocaleContentFromPkg } from "@/api/services/utils";
import { SpinnerLoading } from "@/components/status/loading";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import {
  Tooltip,
  TooltipContent,
  TooltipProvider,
  TooltipTrigger,
} from "@/components/ui/tooltip";
import { cn, compareVersions } from "@/lib/utils";
import { useAppStore, useWidgetStore } from "@/store";
import {
  EPackageSource,
  ETenPackageType,
  type IListTenCloudStorePackage,
  type IListTenLocalStorePackage,
  type ITenPackage,
  type ITenPackageLocal,
  type TenPackageQueryFilterSchema,
  TenPackageTypeMappings,
} from "@/types/extension";
import { ExtensionDetails } from "./extension-details";

export const ExtensionStoreWidget = (props: {
  className?: string;
  toolTipSide?: TooltipContentProps["side"];
}) => {
  const { className } = props;

  const [searchQuery, setSearchQuery] = React.useState("");
  const [showFilters, setShowFilters] = React.useState(false);
  const [selectedType, setSelectedType] = React.useState<
    ETenPackageType | "all" | undefined
  >("all");

  const deferredSearch = React.useDeferredValue(searchQuery.trim());

  const { t } = useTranslation();
  const { setDefaultOsArch } = useAppStore();

  const {
    data: searchedData,
    error: searchedDataError,
    isLoading: isSearchedDataLoading,
  } = useSearchTenCloudStorePackages(
    deferredSearch
      ? {
          filter: {
            or: [
              {
                field: "name",
                operator: "regex",
                value: `.*${deferredSearch}.*`,
              },
              {
                field: "display_name",
                operator: "regex",
                value: `.*${deferredSearch}.*`,
              },
            ],
          },
          options: {
            scope:
              // eslint-disable-next-line max-len
              "name,version,hash,display_name,tags,dependencies,downloadUrl,type,description",
          },
        }
      : undefined
  );
  const { data: envData, error: envError, isLoading: isLoadingEnv } = useEnv();
  const {
    data: addons,
    error: addonError,
    isLoading: isFetchingAddons,
  } = useFetchAddons({});

  console.log({ isFetchingAddons, isLoadingEnv, isSearchedDataLoading });

  const isLoading = React.useMemo(() => {
    return isSearchedDataLoading || isLoadingEnv || isFetchingAddons;
  }, [isFetchingAddons, isLoadingEnv, isSearchedDataLoading]);
  const displayItems = React.useMemo(() => {
    const latestUniqueItems = new Map<string, IListTenCloudStorePackage>();
    searchedData?.packages.forEach((pkg) => {
      const existing = latestUniqueItems.get(pkg.name);
      if (!existing || compareVersions(pkg.version, existing.version) > 0) {
        latestUniqueItems.set(pkg.name, pkg);
      }
    });
    return Array.from(latestUniqueItems.values());
  }, [searchedData?.packages]);

  const typeCounts: Record<ETenPackageType, number> = React.useMemo(() => {
    return {
      [ETenPackageType.Extension]: 0,
      [ETenPackageType.App]: 0,
      [ETenPackageType.AddonLoader]: 0,
      [ETenPackageType.System]: 0,
      [ETenPackageType.Protocol]: 0,
    };
  }, []);

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
  }, [addonError, envError, searchedDataError, t]);

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
          value={searchQuery}
          onChange={(e) => setSearchQuery(e.target.value)}
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
                  {Object.values(typeCounts).reduce((a, b) => a + b, 0)})
                </TabsTrigger>
                {Object.entries(TenPackageTypeMappings).map(([key, value]) => (
                  <TabsTrigger key={key} value={key} className="px-1 text-xs">
                    <value.icon className="size-3" />
                    {typeCounts[key as ETenPackageType]}
                  </TabsTrigger>
                ))}
              </TabsList>
            </Tabs>
          </motion.div>
        )}
      </AnimatePresence>

      <div className="my-auto">
        <AnimatePresence mode="popLayout">
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
          <div className="flex flex-col gap-2 py-2">
            {displayItems.map((item) => (
              <ExtensionItem
                key={item.hash}
                item={{
                  ...item,
                  _type: EPackageSource.Default,

                  isInstalled: addons.some((addon) => addon.name === item.name),
                }}
                variant={item.type}
              />
            ))}
          </div>
          {!isLoading && displayItems.length === 0 && (
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
        </AnimatePresence>
      </div>

      {/* Footer */}
      <div className="pt-3">
        <div
          className={cn(
            "flex items-center justify-between text-ten-icontext-2 text-xs"
          )}
        >
          <span></span>
          <span>TEN Store</span>
        </div>
      </div>
    </div>
  );
};

const extensionListItemVariants = cva("", {
  variants: {
    text: {
      [ETenPackageType.AddonLoader]: "text-blue-800 dark:text-blue-200",
      [ETenPackageType.App]: "text-green-800 dark:text-green-200",
      [ETenPackageType.Extension]: "text-purple-800 dark:text-purple-200",
      [ETenPackageType.Protocol]: "text-orange-800 dark:text-orange-200",
      [ETenPackageType.System]: "text-red-800 dark:text-red-200",
    },
    bg: {
      [ETenPackageType.AddonLoader]: "bg-blue-100 dark:bg-blue-900",
      [ETenPackageType.App]: "bg-green-100 dark:bg-green-900",
      [ETenPackageType.Extension]: "bg-purple-100 dark:bg-purple-900",
      [ETenPackageType.Protocol]: "bg-orange-100 dark:bg-orange-900",
      [ETenPackageType.System]: "bg-red-100 dark:bg-red-900",
    },
  },
});

const ExtensionItem = (props: {
  item: ITenPackage | ITenPackageLocal;
  variant?: ETenPackageType;
  className?: string;
}) => {
  const { item, variant = ETenPackageType.Extension, className } = props;

  const { t, i18n } = useTranslation();
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

  return (
    <motion.div
      layout
      initial={{ opacity: 0, y: 20 }}
      animate={{ opacity: 1, y: 0 }}
      exit={{ opacity: 0, y: -20 }}
      transition={{ duration: 0.2 }}
      className={cn(
        "h-24 cursor-pointer rounded-lg p-3 transition-[colors,box-shadow]",
        "inset-shadow-xs shadow-xs hover:shadow-md hover:ring-1",
        extensionListItemVariants({ text: variant }),
        className
      )}
    >
      <div className="flex h-full items-start gap-3">
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
            <Button
              size="xs"
              disabled={isInstalled}
              variant={isInstalled ? "outline" : "default"}
              onClick={(e) => {
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
                  <span className="sr-only">{t("extensionStore.install")}</span>
                </>
              )}
            </Button>
          )}
        </div>
      </div>
    </motion.div>
  );
};
