//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as React from "react";
import { BlocksIcon, CheckIcon } from "lucide-react";
import { useTranslation } from "react-i18next";
import { FixedSizeList as VirtualList } from "react-window";
import AutoSizer from "react-virtualized-auto-sizer";

import { Button } from "@/components/ui/Button";
import {
  Tooltip,
  TooltipContent,
  TooltipProvider,
  TooltipTrigger,
} from "@/components/ui/Tooltip";
import { cn } from "@/lib/utils";
import { useWidgetStore, useAppStore } from "@/store";
import {
  ELogViewerScriptType,
  EWidgetCategory,
  EWidgetDisplayType,
} from "@/types/widgets";
// eslint-disable-next-line max-len
import { ExtensionTooltipContent } from "@/components/Widget/ExtensionWidget/ExtensionDetails";
import {
  type IListTenCloudStorePackage,
  type IListTenLocalStorePackage,
  EPackageSource,
  ITenPackage,
  ITenPackageLocal,
} from "@/types/extension";
import { useListTenCloudStorePackages } from "@/api/services/extension";
import { TEN_PATH_WS_BUILTIN_FUNCTION } from "@/constants";
import { getWSEndpointFromWindow } from "@/constants/utils";

import type { TooltipContentProps } from "@radix-ui/react-tooltip";
import { postReloadApps } from "@/api/services/apps";
import {
  EXTENSION_WIDGET_ID,
  GROUP_EXTENSION_ID,
  GROUP_LOG_VIEWER_ID,
} from "@/constants/widgets";
import { CONTAINER_DEFAULT_ID } from "@/constants/widgets";
import { LogViewerPopupTitle } from "@/components/Popup/LogViewer";
import { ExtensionPopupTitle } from "@/components/Popup/Default/Extension";

export const ExtensionList = (props: {
  items: (ITenPackage | ITenPackageLocal)[];
  versions: Map<string, IListTenCloudStorePackage[]>;
  className?: string;
  toolTipSide?: TooltipContentProps["side"];
  readOnly?: boolean;
}) => {
  const { items, versions, className, toolTipSide, readOnly } = props;

  const VirtualListItem = (props: {
    index: number;
    style: React.CSSProperties;
  }) => {
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

  return (
    <div className={cn("w-full h-full", className)}>
      <AutoSizer>
        {({ width, height }: { width: number; height: number }) => (
          <VirtualList
            width={width}
            height={height}
            itemCount={items.length}
            itemSize={52}
          >
            {VirtualListItem}
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

  const { t } = useTranslation();
  const {
    appendWidgetIfNotExists,
    removeBackstageWidget,
    removeLogViewerHistory,
  } = useWidgetStore();
  const { currentWorkspace } = useAppStore();
  const { mutate } = useListTenCloudStorePackages();

  const handleInstall =
    (baseDir: string, item: IListTenCloudStorePackage) =>
    (e: React.MouseEvent) => {
      e.preventDefault();
      e.stopPropagation();
      if (!baseDir || !item) {
        return;
      }
      const widgetId = "ext-install-" + item.hash;
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
            mutate();
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

  return (
    <li
      ref={ref}
      className={cn(
        "px-1 py-2",
        "flex gap-2 w-full items-center max-w-full font-roboto h-fit",
        "hover:bg-gray-100 dark:hover:bg-gray-800",
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
          "flex flex-col  justify-between",
          "w-full overflow-hidden text-sm"
        )}
      >
        <h3
          className={cn(
            "font-semibold w-full overflow-hidden text-ellipsis",
            "text-gray-900 dark:text-gray-100"
          )}
        >
          {item.name}
          {_type === EPackageSource.Local && (
            <span
              className={cn(
                "text-gray-500 dark:text-gray-400",
                "text-xs font-normal"
              )}
            >
              {t("extensionStore.localAddonTip")}
            </span>
          )}
        </h3>
        <p className={cn("text-xs text-gray-500 dark:text-gray-400")}>
          {item.type}
        </p>
      </div>
      <div className="mt-auto flex flex-col items-end">
        {isInstalled ? (
          <>
            <Button
              variant="ghost"
              size="icon"
              disabled
              className={cn(
                "size-4 [&_svg]:size-3  cursor-pointer",
                "hover:bg-gray-200 dark:hover:bg-gray-700"
              )}
            >
              <CheckIcon className="" />
            </Button>
          </>
        ) : (
          <Button
            variant="secondary"
            size="sm"
            className={cn(
              "text-xs px-2 py-0.5 font-normal h-fit cursor-pointer",
              "shadow-none rounded-none",
              "hover:bg-gray-200 dark:hover:bg-gray-700"
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

  const { appendWidgetIfNotExists } = useWidgetStore();

  const handleClick = () => {
    appendWidgetIfNotExists({
      container_id: CONTAINER_DEFAULT_ID,
      group_id: GROUP_EXTENSION_ID,
      widget_id: EXTENSION_WIDGET_ID + "-" + item.name,

      category: EWidgetCategory.Extension,
      display_type: EWidgetDisplayType.Popup,

      title: <ExtensionPopupTitle name={item.name} />,
      metadata: {
        name: item.name,
        versions: versions || [],
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
            "backdrop-blur-xs shadow-xl text-popover-foreground",
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
