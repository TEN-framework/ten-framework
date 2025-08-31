//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import {
  EllipsisVerticalIcon,
  PanelBottomIcon,
  PanelLeftIcon,
  PanelRightIcon,
  SquareArrowOutUpRightIcon,
  XIcon,
} from "lucide-react";
// import TerminalWidget from "@/components/Widget/TerminalWidget";
// import EditorWidget from "@/components/Widget/EditorWidget";
// eslint-disable-next-line max-len
// import { LogViewerFrontStageWidget } from "@/components/Widget/LogViewerWidget";
// import { ExtensionStoreWidget } from "@/components/Widget/ExtensionWidget";
import { motion } from "motion/react";
import type * as React from "react";
import { useTranslation } from "react-i18next";
import {
  ContextMenu,
  ContextMenuContent,
  ContextMenuItem,
  ContextMenuSeparator,
  ContextMenuShortcut,
  ContextMenuTrigger,
} from "@/components/ui/context-menu";
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuLabel,
  DropdownMenuRadioGroup,
  DropdownMenuRadioItem,
  DropdownMenuSeparator,
  DropdownMenuTrigger,
} from "@/components/ui/dropdown-menu";
import { cn } from "@/lib/utils";
// import { useWidgetStore, useDialogStore } from "@/store";
import type {
  //   EWidgetCategory,
  //   EWidgetDisplayType,
  //   IEditorWidget,
  //   EDefaultWidgetType,
  IWidget,
  //   ILogViewerWidget,
  //   IEditorWidgetRef,
} from "@/types/widgets";

export interface IDockBaseProps {
  children?: React.ReactNode;
  className?: string;
}

/** @deprecated */
export const DockBase = (props: IDockBaseProps) => {
  const { children, className } = props;

  return (
    <div
      className={cn("h-full w-full bg-muted text-muted-foreground", className)}
    >
      {children}
    </div>
  );
};

/** @deprecated */
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
        "flex h-6 w-full items-center justify-between",
        "px-4",
        "bg-border dark:bg-popover",
        className
      )}
    >
      {children}
      {/* Action Bar */}
      <div className="flex w-fit items-center gap-2">
        <DropdownMenu>
          <DropdownMenuTrigger>
            <EllipsisVerticalIcon className="h-4 w-4" />
          </DropdownMenuTrigger>
          <DropdownMenuContent>
            <DropdownMenuLabel>{t("dock.dockSide")}</DropdownMenuLabel>
            <DropdownMenuSeparator />
            <DropdownMenuRadioGroup
              value={position}
              onValueChange={onPositionChange}
            >
              <DropdownMenuRadioItem value="left">
                <PanelLeftIcon className="me-2 h-4 w-4" />
                {t("dock.left")}
              </DropdownMenuRadioItem>
              <DropdownMenuRadioItem value="right">
                <PanelRightIcon className="me-2 h-4 w-4" />
                {t("dock.right")}
              </DropdownMenuRadioItem>
              <DropdownMenuRadioItem value="bottom">
                <PanelBottomIcon className="me-2 h-4 w-4" />
                {t("dock.bottom")}
              </DropdownMenuRadioItem>
            </DropdownMenuRadioGroup>
          </DropdownMenuContent>
        </DropdownMenu>
        {onClose && (
          <XIcon
            className="h-4 w-4 cursor-pointer"
            type="button"
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
        <motion.div
          className={cn(
            "w-fit px-2 py-1 text-muted-foreground text-xs",
            "flex cursor-pointer items-center gap-1",
            "border-transparent border-b-2",
            {
              "text-primary": selected,
              "border-purple-900": selected,
            },
            "hover:border-purple-950 hover:text-primary"
          )}
          onClick={() => onSelect?.(widget.widget_id)}
        >
          {title}
          {hasUnsavedChanges && (
            <span className="font-sans text-foreground/50 text-sm">*</span>
          )}
          {onClose && (
            <XIcon
              className="size-3"
              onClick={() => onClose(widget.widget_id)}
            />
          )}
        </motion.div>
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
