//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as React from "react";
import {
  XIcon,
  EllipsisVerticalIcon,
  PanelRightIcon,
  PanelLeftIcon,
  PanelBottomIcon,
  SquareArrowOutUpRightIcon,
} from "lucide-react";
import { useTranslation } from "react-i18next";

import { cn } from "@/lib/utils";
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuLabel,
  DropdownMenuRadioGroup,
  DropdownMenuRadioItem,
  DropdownMenuSeparator,
  DropdownMenuTrigger,
} from "@/components/ui/DropdownMenu";
import {
  ContextMenu,
  ContextMenuContent,
  ContextMenuItem,
  ContextMenuSeparator,
  ContextMenuShortcut,
  ContextMenuTrigger,
} from "@/components/ui/ContextMenu";
// import { useWidgetStore, useDialogStore } from "@/store";
import {
  //   EWidgetCategory,
  //   EWidgetDisplayType,
  //   IEditorWidget,
  //   EDefaultWidgetType,
  IWidget,
  //   ILogViewerWidget,
  //   IEditorWidgetRef,
} from "@/types/widgets";
// import TerminalWidget from "@/components/Widget/TerminalWidget";
// import EditorWidget from "@/components/Widget/EditorWidget";
// eslint-disable-next-line max-len
// import { LogViewerFrontStageWidget } from "@/components/Widget/LogViewerWidget";
// import { ExtensionStoreWidget } from "@/components/Widget/ExtensionWidget";

export interface IDockBaseProps {
  children?: React.ReactNode;
  className?: string;
}

export const DockBase = (props: IDockBaseProps) => {
  const { children, className } = props;

  return (
    <div
      className={cn("w-full h-full bg-muted text-muted-foreground", className)}
    >
      {children}
    </div>
  );
};

export const DockHeader = (props: {
  position?: string;
  className?: string;
  onPositionChange?: (position: string) => void;
  children?: React.ReactNode;
  onClose?: () => void;
}) => {
  const {
    position = "bottom",
    className,
    onPositionChange,
    children,
    onClose,
  } = props;

  const { t } = useTranslation();

  return (
    <div
      className={cn(
        "w-full h-6 flex items-center justify-between",
        "px-4",
        "bg-border dark:bg-popover",
        className
      )}
    >
      {children}
      {/* Action Bar */}
      <div className="w-fit flex items-center gap-2">
        <DropdownMenu>
          <DropdownMenuTrigger>
            <EllipsisVerticalIcon className="w-4 h-4" />
          </DropdownMenuTrigger>
          <DropdownMenuContent>
            <DropdownMenuLabel>{t("dock.dockSide")}</DropdownMenuLabel>
            <DropdownMenuSeparator />
            <DropdownMenuRadioGroup
              value={position}
              onValueChange={onPositionChange}
            >
              <DropdownMenuRadioItem value="left">
                <PanelLeftIcon className="w-4 h-4 me-2" />
                {t("dock.left")}
              </DropdownMenuRadioItem>
              <DropdownMenuRadioItem value="right">
                <PanelRightIcon className="w-4 h-4 me-2" />
                {t("dock.right")}
              </DropdownMenuRadioItem>
              <DropdownMenuRadioItem value="bottom">
                <PanelBottomIcon className="w-4 h-4 me-2" />
                {t("dock.bottom")}
              </DropdownMenuRadioItem>
            </DropdownMenuRadioGroup>
          </DropdownMenuContent>
        </DropdownMenu>
        {onClose && (
          <XIcon
            className="w-4 h-4 cursor-pointer"
            type="button"
            tabIndex={1}
            onClick={onClose}
          />
        )}
      </div>
    </div>
  );
};

export const DockerHeaderTabElement = (props: {
  widget: IWidget;
  selected?: boolean;
  hasUnsavedChanges?: boolean;
  onClose?: (id: string) => void;
  onPopout?: (id: string) => void;
  onSelect?: (id: string) => void;
}) => {
  const { widget, selected, hasUnsavedChanges, onClose, onPopout, onSelect } =
    props;
  const title =
    (widget.metadata as unknown as { title?: string })?.title ??
    widget.category;
  const { t } = useTranslation();

  return (
    <ContextMenu>
      <ContextMenuTrigger>
        <div
          className={cn(
            "w-fit px-2 py-1 text-xs text-muted-foreground",
            "flex items-center gap-1 cursor-pointer",
            "border-b-2 border-transparent",
            {
              "text-primary": selected,
              "border-purple-900": selected,
            },
            "hover:text-primary hover:border-purple-950"
          )}
          onClick={() => onSelect?.(widget.widget_id)}
        >
          {title}
          {hasUnsavedChanges && (
            <span className="text-foreground/50 text-sm font-sans">*</span>
          )}
          {onClose && (
            <XIcon
              className="size-3"
              onClick={() => onClose(widget.widget_id)}
            />
          )}
        </div>
      </ContextMenuTrigger>
      <ContextMenuContent>
        <ContextMenuItem
          onClick={() => {
            onPopout?.(widget.widget_id);
          }}
        >
          {t("action.popout")}
          <ContextMenuShortcut>
            <SquareArrowOutUpRightIcon className="size-3" />
          </ContextMenuShortcut>
        </ContextMenuItem>
        <ContextMenuSeparator />
        <ContextMenuItem onClick={() => onClose?.(widget.widget_id)}>
          {t("action.close")}
        </ContextMenuItem>
      </ContextMenuContent>
    </ContextMenu>
  );
};
