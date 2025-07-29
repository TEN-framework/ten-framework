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
  CheckIcon,
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
import { useFetchAddons } from "@/api/services/addons";
import { useEnv } from "@/api/services/common";
import {
  useListTenCloudStorePackages,
  useSearchTenCloudStorePackages,
} from "@/api/services/extension";
import { SpinnerLoading } from "@/components/status/loading";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { cn, compareVersions } from "@/lib/utils";
import { useAppStore, useWidgetStore } from "@/store";
import {
  EPackageSource,
  type IListTenCloudStorePackage,
  type IListTenLocalStorePackage,
  type ITenPackage,
  type ITenPackageLocal,
} from "@/types/extension";
import { ExtensionDetails } from "./extension-details";

// Extension type categories
const extensionTypeColors = {
  extension:
    "bg-blue-100 text-blue-800 border-blue-200 dark:bg-blue-900 dark:text-blue-200",
  app: "bg-green-100 text-green-800 border-green-200 dark:bg-green-900 dark:text-green-200",
  protocol:
    "bg-purple-100 text-purple-800 border-purple-200 dark:bg-purple-900 dark:text-purple-200",
  addon:
    "bg-orange-100 text-orange-800 border-orange-200 dark:bg-orange-900 dark:text-orange-200",
};

const extensionTypeIcons = {
  extension: <BlocksIcon className="h-3 w-3" />,
  app: <ChromeIcon className="h-3 w-3" />,
  protocol: <SettingsIcon className="h-3 w-3" />,
  addon: <BlocksIcon className="h-3 w-3" />,
};

interface ExtensionItemProps {
  item: ITenPackage | ITenPackageLocal;
  versions?: IListTenCloudStorePackage[];
  onAction?: (item: ITenPackage | ITenPackageLocal) => void;
}

const ExtensionItem = ({ item, versions, onAction }: ExtensionItemProps) => {
  const { t, i18n } = useTranslation();
  const isInstalled = item.isInstalled;

  const prettyName = React.useMemo(() => {
    if (item._type === EPackageSource.Local) {
      return item.name;
    }
    return (item as ITenPackage).name; // Could add display_name extraction here
  }, [item]);

  const prettyDescription = React.useMemo(() => {
    if (item._type === EPackageSource.Local) {
      return item.type || "";
    }
    return (item as ITenPackage).type || ""; // Could add description extraction here
  }, [item]);

  return (
    <motion.div
      layout
      initial={{ opacity: 0, y: 20 }}
      animate={{ opacity: 1, y: 0 }}
      exit={{ opacity: 0, y: -20 }}
      transition={{ duration: 0.2 }}
      className="cursor-pointer rounded-lg border border-border p-3 transition-colors hover:bg-accent/50"
      onClick={() => onAction?.(item)}
    >
      <div className="flex items-start gap-3">
        {/* Icon */}
        <div className="flex-shrink-0">
          <BlocksIcon className="h-8 w-8 text-primary" />
        </div>

        {/* Content */}
        <div className="min-w-0 flex-1">
          <div className="mb-1 flex items-center gap-2">
            <h3 className="truncate font-medium text-foreground text-sm">
              {prettyName}
            </h3>
            <Badge
              variant="outline"
              className={cn(
                "h-5 px-1.5 py-0.5 text-xs",
                extensionTypeColors[
                  item.type as keyof typeof extensionTypeColors
                ] || extensionTypeColors.extension
              )}
            >
              {extensionTypeIcons[
                item.type as keyof typeof extensionTypeIcons
              ] || extensionTypeIcons.extension}
              <span className="ml-1 capitalize">{item.type}</span>
            </Badge>
            {item._type === EPackageSource.Local && (
              <Badge variant="secondary" className="h-5 px-1.5 py-0.5 text-xs">
                Local
              </Badge>
            )}
          </div>

          <p className="mb-2 line-clamp-2 text-muted-foreground text-xs">
            {prettyDescription}
          </p>

          <div className="flex items-center gap-3 text-muted-foreground text-xs">
            {item._type !== EPackageSource.Local && (
              <>
                <div className="flex items-center gap-1">
                  <StarIcon className="h-3 w-3 fill-yellow-400 text-yellow-400" />
                  <span>4.5</span>
                </div>
                <div className="flex items-center gap-1">
                  <DownloadIcon className="h-3 w-3" />
                  <span>1K+</span>
                </div>
              </>
            )}
          </div>
        </div>

        {/* Action Button */}
        <div className="flex-shrink-0">
          <Button
            size="sm"
            variant={isInstalled ? "outline" : "default"}
            className="h-7 px-3 text-xs"
            onClick={(e) => {
              e.stopPropagation();
              onAction?.(item);
            }}
          >
            {isInstalled ? (
              <>
                <CheckIcon className="mr-1 h-3 w-3" />
                Installed
              </>
            ) : (
              <>
                <DownloadIcon className="mr-1 h-3 w-3" />
                Install
              </>
            )}
          </Button>
        </div>
      </div>
    </motion.div>
  );
};

export const ExtensionStoreWidget = (props: {
  className?: string;
  toolTipSide?: TooltipContentProps["side"];
}) => {
  const { className } = props;
  const { t } = useTranslation();

  // State for search and filters
  const [searchQuery, setSearchQuery] = React.useState("");
  const [selectedType, setSelectedType] = React.useState<string>("all");
  const [showFilters, setShowFilters] = React.useState(false);
  const [isInitialLoad, setIsInitialLoad] = React.useState(true);

  // API hooks - initial load with deprecated API
  const {
    data: initialData,
    error: initialError,
    isLoading: isInitialLoading,
  } = useListTenCloudStorePackages();

  // Search API for subsequent searches
  const searchFilter = React.useMemo(() => {
    if (isInitialLoad && !searchQuery.trim() && selectedType === "all") {
      return null; // Don't use search API for initial load
    }

    return {
      name: searchQuery.trim() || undefined,
      type: selectedType !== "all" ? [selectedType] : undefined,
    };
  }, [searchQuery, selectedType, isInitialLoad]);

  const {
    data: searchData,
    error: searchError,
    isLoading: isSearchLoading,
  } = useSearchTenCloudStorePackages(searchFilter || { name: undefined }, {
    page: 1,
    page_size: 100,
  });

  const { data: envData, error: envError, isLoading: isLoadingEnv } = useEnv();
  const { setDefaultOsArch } = useAppStore();
  const {
    data: addons,
    error: addonError,
    isLoading: isFetchingAddons,
  } = useFetchAddons({});

  // Update initial load flag when user starts searching
  React.useEffect(() => {
    if (searchQuery.trim() || selectedType !== "all") {
      setIsInitialLoad(false);
    }
  }, [searchQuery, selectedType]);

  // Choose data source based on whether it's initial load or search
  const rawData = React.useMemo(() => {
    if (isInitialLoad) {
      return initialData;
    }
    return { packages: searchData || [] };
  }, [isInitialLoad, initialData, searchData]);

  const isLoading =
    isInitialLoading || isSearchLoading || isFetchingAddons || isLoadingEnv;
  const error = initialError || searchError || envError || addonError;

  // Process extensions data
  const [processedExtensions, versions] = React.useMemo(() => {
    const cloudExtNames = rawData?.packages?.map((item) => item.name) || [];
    const [localOnlyAddons, otherAddons] = addons.reduce(
      ([localOnly, other], addon) => {
        if (cloudExtNames.includes(addon.name)) {
          other.push(addon);
        } else {
          localOnly.push(addon);
        }
        return [localOnly, other];
      },
      [[], []] as [IListTenLocalStorePackage[], IListTenLocalStorePackage[]]
    );

    // Create versions map
    const versions = new Map<string, IListTenCloudStorePackage[]>();
    rawData?.packages?.forEach((item) => {
      if (versions.has(item.name)) {
        const version = versions.get(item.name);
        if (version) {
          version.push(item);
          version.sort((a, b) => compareVersions(b.version, a.version));
        }
      } else {
        versions.set(item.name, [item]);
      }
    });

    // Process packages
    const [installedPackages, uninstalledPackages] = (
      rawData?.packages || []
    ).reduce(
      ([installed, uninstalled], item) => {
        const packageName = item.name;
        const isInstalled = otherAddons.some(
          (addon) => addon.name === packageName
        );
        if (isInstalled) {
          installed.push(item);
        } else {
          uninstalled.push(item);
        }
        return [installed, uninstalled];
      },
      [[], []] as [IListTenCloudStorePackage[], IListTenCloudStorePackage[]]
    );

    const processedData = [
      ...(localOnlyAddons.map((item) => ({
        ...item,
        isInstalled: true,
        _type: EPackageSource.Local,
      })) as ITenPackageLocal[]),
      ...(installedPackages.map((item) => ({
        ...item,
        isInstalled: true,
        _type: EPackageSource.Default,
      })) as ITenPackage[]),
      ...(uninstalledPackages.map((item) => ({
        ...item,
        isInstalled: false,
        _type: EPackageSource.Default,
      })) as ITenPackage[]),
    ];

    return [processedData, versions];
  }, [rawData?.packages, addons]);

  // Filter extensions
  const filteredExtensions = React.useMemo(() => {
    let filtered = processedExtensions;

    // For initial load, apply client-side filtering if needed
    if (isInitialLoad) {
      if (searchQuery.trim()) {
        filtered = filtered.filter((ext) =>
          ext.name.toLowerCase().includes(searchQuery.toLowerCase())
        );
      }

      if (selectedType !== "all") {
        filtered = filtered.filter((ext) => ext.type === selectedType);
      }
    }

    return filtered;
  }, [processedExtensions, searchQuery, selectedType, isInitialLoad]);

  // Get type counts for tabs
  const getTypeCounts = React.useMemo(() => {
    const counts = {
      all: processedExtensions.length,
      extension: processedExtensions.filter((e) => e.type === "extension")
        .length,
      app: processedExtensions.filter((e) => e.type === "app").length,
      protocol: processedExtensions.filter((e) => e.type === "protocol").length,
      addon: processedExtensions.filter((e) => e.type === "addon").length,
    };
    return counts;
  }, [processedExtensions]);

  const handleExtensionAction = (item: ITenPackage | ITenPackageLocal) => {
    // Handle extension install/manage actions
    console.log("Extension action:", item);
  };

  // Error handling
  React.useEffect(() => {
    if (error) {
      toast.error(error.message, {
        description: error?.message,
      });
    }
  }, [error]);

  React.useEffect(() => {
    if (envData?.os && envData?.arch) {
      setDefaultOsArch({ os: envData.os, arch: envData.arch });
    }
  }, [envData?.os, envData?.arch, setDefaultOsArch]);

  if (isLoading) {
    return <SpinnerLoading className="mx-auto" />;
  }

  return (
    <div
      className={cn(
        "flex h-[600px] w-[420px] flex-col rounded-lg border border-border bg-background shadow-lg",
        className
      )}
    >
      {/* Header */}
      <div className="border-border border-b p-4">
        <div className="mb-3 flex items-center gap-2">
          <ChromeIcon className="h-5 w-5 text-primary" />
          <h1 className="font-semibold text-foreground text-lg">
            {t("extensionStore.title", { defaultValue: "Extensions" })}
          </h1>
        </div>

        {/* Search Bar */}
        <div className="relative">
          <SearchIcon className="-translate-y-1/2 absolute top-1/2 left-3 h-4 w-4 transform text-muted-foreground" />
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
            className="-translate-y-1/2 absolute top-1/2 right-1 h-7 w-7 transform p-0"
            onClick={() => setShowFilters(!showFilters)}
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
              <Tabs value={selectedType} onValueChange={setSelectedType}>
                <TabsList className="grid h-8 w-full grid-cols-5">
                  <TabsTrigger value="all" className="px-2 text-xs">
                    All ({getTypeCounts.all})
                  </TabsTrigger>
                  <TabsTrigger value="extension" className="px-1 text-xs">
                    <BlocksIcon className="mr-1 h-3 w-3" />
                    {getTypeCounts.extension}
                  </TabsTrigger>
                  <TabsTrigger value="app" className="px-1 text-xs">
                    <ChromeIcon className="mr-1 h-3 w-3" />
                    {getTypeCounts.app}
                  </TabsTrigger>
                  <TabsTrigger value="protocol" className="px-1 text-xs">
                    <SettingsIcon className="mr-1 h-3 w-3" />
                    {getTypeCounts.protocol}
                  </TabsTrigger>
                  <TabsTrigger value="addon" className="px-1 text-xs">
                    <BlocksIcon className="mr-1 h-3 w-3" />
                    {getTypeCounts.addon}
                  </TabsTrigger>
                </TabsList>
              </Tabs>
            </motion.div>
          )}
        </AnimatePresence>
      </div>

      {/* Extensions List */}
      <div className="flex-1 overflow-y-auto">
        <AnimatePresence mode="popLayout">
          {filteredExtensions.length === 0 ? (
            <motion.div
              initial={{ opacity: 0 }}
              animate={{ opacity: 1 }}
              exit={{ opacity: 0 }}
              className="flex h-full flex-col items-center justify-center p-6 text-center"
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
          ) : (
            <motion.div layout className="p-2">
              {filteredExtensions.map((extension, index) => (
                <ExtensionItem
                  key={`${extension._type}-${extension.name}`}
                  item={extension}
                  versions={versions.get(extension.name)}
                  onAction={handleExtensionAction}
                />
              ))}
            </motion.div>
          )}
        </AnimatePresence>
      </div>

      {/* Footer */}
      <div className="border-border border-t bg-muted/30 p-3">
        <div className="flex items-center justify-between text-muted-foreground text-xs">
          <span>{filteredExtensions.length} extensions</span>
          <span>TEN Store</span>
        </div>
      </div>
    </div>
  );
};

export const ExtensionWidget = (props: {
  className?: string;
  versions: IListTenCloudStorePackage[];
  name: string;
}) => {
  const { className, versions, name } = props;

  if (versions?.length === 0) {
    return null;
  }

  return (
    <ExtensionDetails versions={versions} name={name} className={className} />
  );
};
