//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
/* eslint-disable react-refresh/only-export-components */
import React from "react";
import { ChevronRightIcon } from "lucide-react";

import { Button, ButtonProps } from "@/components/ui/Button";
import {
  Tooltip,
  TooltipContent,
  TooltipProvider,
  TooltipTrigger,
} from "@/components/ui/Tooltip";
import { Separator } from "@/components/ui/Separator";
import { cn } from "@/lib/utils";

export enum EContextMenuItemType {
  BUTTON = "button",
  SEPARATOR = "separator",
  LABEL = "label",
  SUB_MENU_BUTTON = "sub_menu_button",
}

export interface IContextMenuItemBase {
  _type: EContextMenuItemType;
  shortcut?: string | React.ReactNode;
}

export interface IContextMenuButtonItem
  extends IContextMenuItemBase,
    ButtonProps {
  _type: EContextMenuItemType.BUTTON;
  label?: string;
  icon?: React.ReactNode;
}

export interface IContextMenuSeparatorItem extends IContextMenuItemBase {
  _type: EContextMenuItemType.SEPARATOR;
}

export interface IContextMenuLabelItem extends IContextMenuItemBase {
  _type: EContextMenuItemType.LABEL;
  label?: string | React.ReactNode;
}

export interface IContextSubMenuButtonItem
  extends IContextMenuItemBase,
    ButtonProps {
  _type: EContextMenuItemType.SUB_MENU_BUTTON;
  label?: string;
  icon?: React.ReactNode;
  items: IContextMenuItem[];
}

export type IContextMenuItem =
  | IContextMenuButtonItem
  | IContextMenuSeparatorItem
  | IContextMenuLabelItem
  | IContextSubMenuButtonItem;

interface ContextMenuProps {
  visible: boolean;
  x: number;
  y: number;
  items: IContextMenuItem[];
}

export const ContextItem = (props: IContextMenuItem) => {
  if (props._type === EContextMenuItemType.BUTTON) {
    return (
      <Button
        variant="ghost"
        className={cn(
          "flex w-full justify-start px-2.5 py-1.5 whitespace-nowrap",
          "h-auto font-normal cursor-pointer"
        )}
        {...props}
      >
        <span
          className={cn(
            "flex items-center shrink-0 justify-center",
            "mr-2 h-[1em] w-5"
          )}
        >
          {props.icon || null}
        </span>
        <span className="flex-1 text-left">{props.label}</span>
        <div className="min-w-4 ml-auto">
          {props.shortcut && (
            <span className="text-xs text-muted-foreground">
              {props.shortcut}
            </span>
          )}
        </div>
      </Button>
    );
  }
  if (props._type === EContextMenuItemType.SEPARATOR) {
    return <Separator className="my-1" />;
  }
  if (props._type === EContextMenuItemType.LABEL) {
    return (
      <div
        className={cn("text-xs font-medium text-muted-foreground", "px-2.5")}
      >
        {props.label}
      </div>
    );
  }
  if (props._type === EContextMenuItemType.SUB_MENU_BUTTON) {
    return (
      <TooltipProvider delayDuration={100}>
        <Tooltip>
          <TooltipTrigger asChild>
            <Button
              variant="ghost"
              asChild
              className={cn(
                "flex w-full justify-start px-2.5 py-1.5 whitespace-nowrap",
                "h-auto font-normal cursor-pointer"
              )}
              {...props}
            >
              <div className="flex items-center">
                <span
                  className={cn(
                    "flex items-center shrink-0 justify-center",
                    "mr-2 h-[1em] w-5"
                  )}
                >
                  {props.icon || null}
                </span>
                <span className="flex-1 text-left">{props.label}</span>
                <ChevronRightIcon className="size-4 ml-auto" />
              </div>
            </Button>
          </TooltipTrigger>
          <TooltipContent
            side="right"
            sideOffset={-10}
            className={cn(
              "w-auto",
              "bg-transparent px-3 py-1.5 text-normal text-foreground"
            )}
          >
            <div
              className={cn(
                "bg-popover shadow-md box-border p-1.5",
                "border border-border rounded-md"
              )}
            >
              {props.items.map((item, index) => (
                <ContextItem key={index} {...item} />
              ))}
            </div>
          </TooltipContent>
        </Tooltip>
      </TooltipProvider>
    );
  }
};

const ContextMenuContainer: React.FC<ContextMenuProps> = ({
  visible,
  x,
  y,
  items,
}) => {
  if (!visible) return null;

  return (
    <div
      className={cn(
        "fixed p-1.5 z-9999",
        "bg-popover border border-border rounded-md shadow-md box-border"
      )}
      style={{
        left: x,
        top: y,
      }}
      onClick={(e) => e.stopPropagation()}
    >
      {items.map((item, index) => (
        <ContextItem key={index} {...item} />
      ))}
    </div>
  );
};

export default ContextMenuContainer;
