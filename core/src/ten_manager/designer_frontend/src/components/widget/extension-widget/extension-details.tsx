import {
  BlocksIcon,
  CheckLineIcon,
  DownloadIcon,
  GitBranchIcon,
  TagIcon,
} from "lucide-react";
import * as React from "react";
import { useTranslation } from "react-i18next";
import Markdown from "react-markdown";
import remarkGfm from "remark-gfm";
import { toast } from "sonner";
import { useFetchAddons } from "@/api/services/addons";
import { useSearchTenCloudStorePackages } from "@/api/services/extension";
import { extractLocaleContentFromPkg } from "@/api/services/utils";
import { SpinnerLoading } from "@/components/status/loading";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import {
  Select,
  SelectContent,
  SelectGroup,
  SelectItem,
  SelectLabel,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
// eslint-disable-next-line max-len
import { extensionListItemVariants } from "@/components/widget/extension-widget/common";
import { cn, compareVersions } from "@/lib/utils";
import {
  type ETenPackageType,
  type IListTenCloudStorePackage,
  TenPackageTypeMappings,
} from "@/types/extension";

// Header Component
const ExtensionHeader: React.FC<{
  selectedPackage: IListTenCloudStorePackage;
  allPackages: IListTenCloudStorePackage[];
  isInstalled: boolean;
  onVersionChange: (versionHash: string) => void;
  onAction: () => void;
}> = ({
  selectedPackage,
  allPackages,
  isInstalled,
  onVersionChange,
  onAction,
}) => {
  const { t, i18n } = useTranslation();

  const displayName = React.useMemo(
    () =>
      extractLocaleContentFromPkg(
        selectedPackage.display_name,
        i18n.language
      ) || selectedPackage.name,
    [selectedPackage.display_name, selectedPackage.name, i18n.language]
  );
  const description = React.useMemo(
    () =>
      extractLocaleContentFromPkg(selectedPackage.description, i18n.language) ||
      "",
    [selectedPackage.description, i18n.language]
  );
  const Icon = React.useMemo(() => {
    return TenPackageTypeMappings[
      selectedPackage.type as keyof typeof TenPackageTypeMappings
    ].icon;
  }, [selectedPackage.type]);

  return (
    <div className="border-b p-6 pb-4">
      <div className="flex items-start gap-4">
        {/* Icon */}
        <div className="flex-shrink-0">
          <div
            className={cn(
              "flex h-16 w-16 items-center justify-center",
              "rounded-lg",
              extensionListItemVariants({
                text: selectedPackage.type,
                bg: selectedPackage.type,
              })
            )}
          >
            <Icon className={cn("size-8")} />
          </div>
        </div>

        {/* Main Info */}
        <div className="flex-1">
          <h1 className={cn("mb-1 font-semibold text-foreground text-xl")}>
            <span>{displayName}</span>
          </h1>
          <p className="mb-3 text-muted-foreground text-sm">{description}</p>

          {/* Stats and Type */}
          {/* <div className="flex items-center gap-4 text-sm">
            <div className="flex items-center gap-1">
              <StarIcon className="h-4 w-4 fill-yellow-400 text-yellow-400" />
              <span className="font-medium">4.5</span>
            </div>
            <div className="flex items-center gap-1">
              <DownloadIcon className="h-4 w-4 text-muted-foreground" />
              <span>1.2K</span>
            </div>
          </div> */}
        </div>

        {/* Version Selector & Action Button */}
        <div className="flex items-start gap-3">
          {/* Version Selector */}
          <div className="min-w-[200px]">
            <h3 className="mb-2 font-medium text-foreground text-xs">
              {t("extensionStore.version")}
            </h3>
            {allPackages.length > 1 ? (
              <Select
                value={selectedPackage.hash}
                onValueChange={onVersionChange}
              >
                <SelectTrigger className="w-full">
                  <SelectValue />
                </SelectTrigger>
                <SelectContent>
                  <SelectGroup>
                    <SelectLabel>
                      {t("extensionStore.selectVersion")}
                    </SelectLabel>
                    {allPackages.map((pkg, index) => (
                      <SelectItem key={pkg.hash} value={pkg.hash}>
                        <div className="flex items-center gap-2">
                          <span className="font-mono">{pkg.version}</span>
                          {index === 0 && (
                            <Badge variant="secondary" className="text-xs">
                              {t("extensionStore.versionLatest")}
                            </Badge>
                          )}
                        </div>
                      </SelectItem>
                    ))}
                  </SelectGroup>
                </SelectContent>
              </Select>
            ) : (
              <div className="flex items-center gap-2">
                <Badge variant="secondary" className="font-mono">
                  {selectedPackage.version}
                </Badge>
                <Badge variant="outline" className="text-xs">
                  {t("extensionStore.versionLatest")}
                </Badge>
              </div>
            )}
          </div>

          {/* Action Button */}
          <div className="flex flex-col items-end">
            <h3 className="mb-2 font-medium text-foreground text-xs opacity-0">
              Action
            </h3>
            <Button
              variant={isInstalled ? "outline" : "default"}
              onClick={onAction}
            >
              {isInstalled ? (
                <>
                  <CheckLineIcon />
                  <span className="">{t("extensionStore.installed")}</span>
                </>
              ) : (
                <>
                  <DownloadIcon />
                  <span className="">{t("extensionStore.install")}</span>
                </>
              )}
            </Button>
          </div>
        </div>
      </div>
    </div>
  );
};

// Content Component
const ExtensionContent: React.FC<{
  selectedPackage: IListTenCloudStorePackage;
  readme: string;
}> = ({ readme }) => {
  return (
    <div className="flex-1">
      {readme ? (
        <div
          className={cn(
            "prose prose-sm max-w-none rounded-lg bg-muted/30 p-4",
            "dark:prose-invert"
          )}
        >
          <Markdown remarkPlugins={[remarkGfm]}>{readme}</Markdown>
        </div>
      ) : (
        <div
          className={cn(
            "flex h-64 items-center justify-center",
            "rounded-lg bg-muted/30"
          )}
        >
          <div className="text-center">
            <h3 className="mb-2 font-medium text-foreground">
              No Documentation Available
            </h3>
            <p className="text-muted-foreground text-sm">
              This extension doesn't have documentation available.
            </p>
          </div>
        </div>
      )}
    </div>
  );
};

// Sidebar Component
const ExtensionSidebar: React.FC<{
  selectedPackage: IListTenCloudStorePackage;
}> = ({ selectedPackage }) => {
  const { t } = useTranslation();

  return (
    <div className="w-80 space-y-6">
      {/* Package Name */}
      <div>
        <h3 className="mb-2 font-medium text-foreground text-sm">
          {t("extensionStore.identifier")}
        </h3>
        <p className="font-mono text-muted-foreground text-sm">
          {selectedPackage.name}
        </p>
      </div>

      {/* Tags */}
      {selectedPackage.tags && selectedPackage.tags.length > 0 && (
        <div>
          <h3
            className={cn(
              "mb-2 flex items-center gap-2",
              "font-medium text-foreground text-sm"
            )}
          >
            <TagIcon className="h-4 w-4" />
            {t("extensionStore.tags")}
          </h3>
          <div className="flex flex-wrap gap-2">
            {selectedPackage.tags.map((tag) => (
              <Badge key={tag} variant="secondary" className="text-xs">
                {tag}
              </Badge>
            ))}
          </div>
        </div>
      )}

      {/* Dependencies */}
      {selectedPackage.dependencies &&
        selectedPackage.dependencies.length > 0 && (
          <div>
            <h3
              className={cn(
                "mb-2 flex items-center gap-2",
                "font-medium text-foreground text-sm"
              )}
            >
              <GitBranchIcon className="h-4 w-4" />
              {t("extensionStore.dependencies")}
            </h3>
            <div className="space-y-2">
              {selectedPackage.dependencies.map((dep) => (
                <div
                  key={dep.name}
                  className={cn(
                    "flex items-center justify-between",
                    "rounded-lg bg-muted p-2"
                  )}
                >
                  <span className="font-medium text-sm">{dep.name}</span>
                  <Badge variant="outline" className="font-mono text-xs">
                    {dep.version}
                  </Badge>
                </div>
              ))}
            </div>
          </div>
        )}

      {/* Supported Platforms */}
      {selectedPackage.supports && selectedPackage.supports.length > 0 && (
        <div>
          <h3 className="mb-2 font-medium text-foreground text-sm">
            {t("extensionStore.supportedPlatforms")}
          </h3>
          <div className="flex flex-wrap gap-2">
            {selectedPackage.supports.map((support) => (
              <Badge
                key={`${support.os}-${support.arch}`}
                variant="outline"
                className="font-mono text-xs"
              >
                {support.os}/{support.arch}
              </Badge>
            ))}
          </div>
        </div>
      )}

      {/* Hash */}
      <div>
        <h3 className="mb-2 font-medium text-foreground text-sm">
          {t("extensionStore.hash")}
        </h3>
        <p
          className={cn(
            "inline-block break-all rounded-lg bg-muted px-2 py-1",
            "font-mono text-muted-foreground text-xs"
          )}
        >
          {selectedPackage.hash}
        </p>
      </div>
    </div>
  );
};

// Loading Component
const ExtensionDetailsLoading: React.FC<{ className?: string }> = ({
  className,
}) => (
  <div
    className={cn("flex h-full w-full items-center justify-center", className)}
  >
    <SpinnerLoading />
  </div>
);

// Not Found Component
const ExtensionDetailsNotFound: React.FC<{ className?: string }> = ({
  className,
}) => (
  <div
    className={cn(
      "flex h-full w-full items-center justify-center p-8",
      className
    )}
  >
    <div className="text-center">
      <BlocksIcon className="mx-auto mb-4 h-12 w-12 text-muted-foreground" />
      <h3 className="mb-2 font-medium text-foreground text-lg">
        Extension not found
      </h3>
      <p className="text-muted-foreground text-sm">
        The requested extension could not be loaded.
      </p>
    </div>
  </div>
);

// Main Component
export const ExtensionDetails = (props: {
  name: string;
  type: ETenPackageType;
  className?: string;
}) => {
  const { name, type, className } = props;

  const [selectedPackage, setSelectedPackage] =
    React.useState<IListTenCloudStorePackage | null>(null);

  const {
    data: searchedData,
    error: searchedDataError,
    isLoading: isSearchedDataLoading,
  } = useSearchTenCloudStorePackages({
    filter: {
      and: [
        {
          field: "name",
          operator: "regex",
          value: `${name}`,
        },
        {
          field: "type",
          operator: "regex",
          value: `${type}`,
        },
      ],
    },
    options: {
      scope:
        "name,version,hash,display_name,tags,dependencies," +
        "downloadUrl,type,description,supports,readme",
    },
  });

  const { data: addons } = useFetchAddons({});
  const { i18n } = useTranslation();

  const [latestPackage, allPackages] = React.useMemo(() => {
    const rawPackages = searchedData?.packages || [];
    const sortedPackages = rawPackages.sort((a, b) =>
      compareVersions(b.version, a.version)
    );
    if (sortedPackages.length === 0) {
      return [null, []];
    }
    return [sortedPackages[0], sortedPackages];
  }, [searchedData]);

  React.useEffect(() => {
    if (selectedPackage) {
      return;
    }
    setSelectedPackage(latestPackage || null);
  }, [latestPackage, selectedPackage]);

  const handleVersionChange = (versionHash: string) => {
    const newSelectedPackage = allPackages.find(
      (pkg) => pkg.hash === versionHash
    );
    if (newSelectedPackage) {
      setSelectedPackage(newSelectedPackage);
    }
  };

  React.useEffect(() => {
    if (searchedDataError) {
      console.error("Failed to fetch extension details:", searchedDataError);
      toast.error(
        `Failed to fetch extension details: ${searchedDataError.message}`
      );
    }
  }, [searchedDataError]);

  const isInstalled = React.useMemo(() => {
    return addons.some((addon) => addon.name === name);
  }, [addons, name]);

  const handleAction = () => {
    if (isInstalled) {
      // Handle manage action
      console.log("Manage extension:", name);
    } else {
      // Handle install action
      console.log("Install extension:", name);
    }
  };

  if (isSearchedDataLoading) {
    return <ExtensionDetailsLoading className={className} />;
  }

  if (!selectedPackage) {
    return <ExtensionDetailsNotFound className={className} />;
  }

  const readme =
    extractLocaleContentFromPkg(selectedPackage.readme, i18n.language) || "";

  return (
    <div
      className={cn(
        "h-full w-full bg-background",
        "overflow-y-auto",
        className
      )}
    >
      <ExtensionHeader
        selectedPackage={selectedPackage}
        allPackages={allPackages}
        isInstalled={isInstalled}
        onVersionChange={handleVersionChange}
        onAction={handleAction}
      />

      <div className="flex-1 overflow-y-auto p-6">
        <div className="flex gap-6">
          <ExtensionContent selectedPackage={selectedPackage} readme={readme} />
          <ExtensionSidebar selectedPackage={selectedPackage} />
        </div>
      </div>
    </div>
  );
};
