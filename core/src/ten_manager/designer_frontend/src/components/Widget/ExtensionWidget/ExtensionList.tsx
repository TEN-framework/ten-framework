//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import type { TooltipContentProps } from "@radix-ui/react-tooltip";
import { BlocksIcon, BrushCleaningIcon, CheckIcon } from "lucide-react";
import * as React from "react";
import { useTranslation } from "react-i18next";
import AutoSizer from "react-virtualized-auto-sizer";
import { FixedSizeList as VirtualList } from "react-window";
import { useFetchAddons } from "@/api/services/addons";
import { postReloadApps } from "@/api/services/apps";
import { useListTenCloudStorePackages } from "@/api/services/extension";
import { extractLocaleContentFromPkg } from "@/api/services/utils";
import { HighlightText } from "@/components/Highlight";
import { ExtensionPopupTitle } from "@/components/Popup/Default/Extension";
import { LogViewerPopupTitle } from "@/components/Popup/LogViewer";
import { Button } from "@/components/ui/Button";
import {
  Tooltip,
  TooltipContent,
  TooltipProvider,
  TooltipTrigger,
} from "@/components/ui/Tooltip";
// eslint-disable-next-line max-len
import { ExtensionTooltipContent } from "@/components/Widget/ExtensionWidget/ExtensionDetails";
import { TEN_PATH_WS_BUILTIN_FUNCTION } from "@/constants";
import { getWSEndpointFromWindow } from "@/constants/utils";
import {
  CONTAINER_DEFAULT_ID,
  EXTENSION_WIDGET_ID,
  GROUP_EXTENSION_ID,
  GROUP_LOG_VIEWER_ID,
} from "@/constants/widgets";
import { cn } from "@/lib/utils";
import { useAppStore, useWidgetStore } from "@/store";
import {
  EPackageSource,
  type IListTenCloudStorePackage,
  type IListTenLocalStorePackage,
  type ITenPackage,
  type ITenPackageLocal,
} from "@/types/extension";
import {
  ELogViewerScriptType,
  EWidgetCategory,
  EWidgetDisplayType,
} from "@/types/widgets";

const VirtualListItem = (props: {
  index: number;
  style: React.CSSProperties;
  items: (ITenPackage | ITenPackageLocal)[];
  versions: Map<string, IListTenCloudStorePackage[]>;
  toolTipSide?: TooltipContentProps["side"];
  readOnly?: boolean;
}) => {
  const { items, versions, toolTipSide, readOnly } = props;
  const item = items[props.index];

  if (item._type === EPackageSource.Local) {
    return (
      <AddonItem
        item={item}
        style={props.style}
        toolTipSide={toolTipSide}
        _type={item._type}
      />
    );
  }

  const targetVersions = versions.get(item.name);

  return (
    <ExtensionStoreItem
      item={item}
      style={props.style}
      toolTipSide={toolTipSide}
      versions={targetVersions}
      isInstalled={item.isInstalled}
      readOnly={readOnly}
    />
  );
};

export const ExtensionList = (props: {
  items: (ITenPackage | ITenPackageLocal)[];
  versions: Map<string, IListTenCloudStorePackage[]>;
  className?: string;
  toolTipSide?: TooltipContentProps["side"];
  readOnly?: boolean;
}) => {
  const { items, versions, className, toolTipSide, readOnly } = props;

  return (
    <div className={cn("h-full w-full", className)}>
      <AutoSizer>
        {({ width, height }: { width: number; height: number }) => (
          <VirtualList
            width={width}
            height={height}
            itemCount={items.length}
            itemSize={52}
          >
            {(virtualProps) => (
              <VirtualListItem
                {...virtualProps}
                items={items}
                versions={versions}
                toolTipSide={toolTipSide}
                readOnly={readOnly}
              />
            )}
          </VirtualList>
        )}
      </AutoSizer>
    </div>
  );
};

export interface IExtensionBaseItemProps<T> {
  item: T;
  _type?: EPackageSource;
  isInstalled?: boolean;
  className?: string;
  readOnly?: boolean;
}
export const ExtensionBaseItem = React.forwardRef<
  HTMLLIElement,
  IExtensionBaseItemProps<
    IListTenCloudStorePackage | IListTenLocalStorePackage
  > &
    React.HTMLAttributes<HTMLLIElement>
>((props, ref) => {
  const { item, className, isInstalled, _type, readOnly, ...rest } = props;

  const { t, i18n } = useTranslation();
  const {
    extSearch,
    appendWidget,
    removeBackstageWidget,
    removeLogViewerHistory,
  } = useWidgetStore();
  const { currentWorkspace } = useAppStore();
  const { mutate: mutatePkgs } = useListTenCloudStorePackages();
  const { mutate: mutateAddons } = useFetchAddons({
    base_dir: currentWorkspace?.app?.base_dir,
  });

  const deferredSearch = React.useDeferredValue(extSearch);

  const [prettyName, prettyDesc] = React.useMemo(() => {
    const name =
      extractLocaleContentFromPkg(
        (item as IListTenCloudStorePackage)?.display_name,
        i18n.language
      ) || item.name;
    const desc = extractLocaleContentFromPkg(
      (item as IListTenCloudStorePackage)?.description,
      i18n.language
    );
    return [name, desc];
  }, [item, i18n.language]);

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
          postActions: () => {
            mutatePkgs();
            mutateAddons();
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

  return (
    <li
      ref={ref}
      className={cn(
        "px-1 py-2",
        "flex h-fit w-full max-w-full items-center gap-2 font-roboto",
        "rounded-sm hover:bg-ten-fill-4",
        {
          "cursor-pointer": !!rest?.onClick && !readOnly,
        },
        className
      )}
      {...rest}
    >
      <BlocksIcon className="size-8" />
      <div
        className={cn(
          "flex flex-col justify-between",
          "w-full overflow-hidden text-sm"
        )}
      >
        <h3
          className={cn(
            "w-full overflow-hidden text-ellipsis font-semibold",
            "text-foreground"
          )}
        >
          <HighlightText highlight={deferredSearch}>{prettyName}</HighlightText>

          {_type === EPackageSource.Local && (
            <span className={cn("text-ten-icontext-2", "font-normal text-xs")}>
              {t("extensionStore.localAddonTip")}
            </span>
          )}
        </h3>
        <p
          className={cn(
            "font-thin text-ten-icontext-2 text-xs",
            "overflow-hidden text-ellipsis"
          )}
        >
          <span className="me-1">{item.type}</span>
          {prettyDesc && (
            <span className={cn("mx-1 font-normal text-xs", "text-nowrap")}>
              {prettyDesc}
            </span>
          )}
        </p>
      </div>
      <div className="my-auto flex flex-col items-end">
        {isInstalled ? (
          <CheckIcon className="size-4" />
        ) : (
          <Button
            variant="outline"
            size="xs"
            className={cn(
              "h-fit cursor-pointer px-2 py-0.5 font-normal text-xs",
              "shadow-none"
            )}
            disabled={readOnly || !currentWorkspace?.app?.base_dir}
            onClick={handleInstall(
              currentWorkspace?.app?.base_dir || "",
              item as IListTenCloudStorePackage
            )}
          >
            {t("extensionStore.install")}
          </Button>
        )}
      </div>
    </li>
  );
});
ExtensionBaseItem.displayName = "ExtensionBaseItem";

export const ExtensionStoreItem = (props: {
  item: IListTenCloudStorePackage;
  versions?: IListTenCloudStorePackage[];
  className?: string;
  style?: React.CSSProperties;
  toolTipSide?: TooltipContentProps["side"];
  isInstalled?: boolean;
  readOnly?: boolean;
}) => {
  const {
    item,
    versions,
    className,
    style,
    toolTipSide = "right",
    isInstalled,
    readOnly,
  } = props;

  const { appendWidget } = useWidgetStore();

  const { i18n } = useTranslation();

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
        />
      ),
      metadata: {
        name: item.name,
        versions: versions || [],
      },
      popup: {
        height: 0.8,
      },
    });
  };

  return (
    <TooltipProvider delayDuration={100}>
      <Tooltip>
        <TooltipTrigger asChild>
          <ExtensionBaseItem
            item={item}
            isInstalled={isInstalled}
            onClick={handleClick}
            className={className}
            style={style}
            readOnly={readOnly}
          />
        </TooltipTrigger>
        <TooltipContent
          side={toolTipSide}
          className={cn(
            "text-popover-foreground shadow-xl backdrop-blur-xs",
            "bg-slate-50/80 dark:bg-gray-900/90",
            "border border-slate-100/50 dark:border-gray-900/50",
            "ring-1 ring-slate-100/50 dark:ring-gray-900/50",
            "font-roboto"
          )}
        >
          <ExtensionTooltipContent item={item} versions={versions || []} />
        </TooltipContent>
      </Tooltip>
    </TooltipProvider>
  );
};

export const AddonItem = (props: {
  item: IListTenLocalStorePackage;
  style?: React.CSSProperties;
  toolTipSide?: TooltipContentProps["side"];
  _type?: EPackageSource;
  readOnly?: boolean;
}) => {
  return (
    <ExtensionBaseItem
      item={props.item}
      isInstalled={true}
      style={props.style}
      _type={props._type}
      readOnly={props.readOnly}
    />
  );
};
