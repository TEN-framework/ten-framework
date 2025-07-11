//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
/* eslint-disable react-refresh/only-export-components */

import type * as DropdownMenuPrimitive from "@radix-ui/react-dropdown-menu";
import { ChevronRightIcon } from "lucide-react";
import type React from "react";
import { Button, type ButtonProps } from "@/components/ui/Button";
import {
  DropdownMenuGroup,
  DropdownMenuItem,
  DropdownMenuLabel,
  DropdownMenuPortal,
  DropdownMenuSeparator,
  DropdownMenuShortcut,
  DropdownMenuSub,
  DropdownMenuSubContent,
  DropdownMenuSubTrigger,
} from "@/components/ui/DropdownMenu";
import { Separator } from "@/components/ui/Separator";
import {
  Tooltip,
  TooltipContent,
  TooltipProvider,
  TooltipTrigger,
} from "@/components/ui/Tooltip";
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

/** @deprecated */
export const ContextItem = (props: IContextMenuItem) => {
  if (props._type === EContextMenuItemType.BUTTON) {
    return (
      <Button
        variant="ghost"
        className={cn(
          "flex w-full justify-start whitespace-nowrap px-2.5 py-1.5",
          "h-auto cursor-pointer font-normal"
        )}
        {...props}
      >
        <span
          className={cn(
            "flex shrink-0 items-center justify-center",
            "mr-2 h-[1em] w-5"
          )}
        >
          {props.icon || null}
        </span>
        <span className="flex-1 text-left">{props.label}</span>
        <div className="ml-auto min-w-4">
          {props.shortcut && (
            <span className="text-muted-foreground text-xs">
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
        className={cn("font-medium text-muted-foreground text-xs", "px-2.5")}
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
                "flex w-full justify-start whitespace-nowrap px-2.5 py-1.5",
                "h-auto cursor-pointer font-normal"
              )}
              {...props}
            >
              <div className="flex items-center">
                <span
                  className={cn(
                    "flex shrink-0 items-center justify-center",
                    "mr-2 h-[1em] w-5"
                  )}
                >
                  {props.icon || null}
                </span>
                <span className="flex-1 text-left">{props.label}</span>
                <ChevronRightIcon className="ml-auto size-4" />
              </div>
            </Button>
          </TooltipTrigger>
          <TooltipContent
            side="right"
            sideOffset={-10}
            className={cn(
              "w-auto",
              "bg-transparent px-3 py-1.5 text-foreground text-normal"
            )}
          >
            <div
              className={cn(
                "box-border bg-popover p-1.5 shadow-md",
                "rounded-md border border-border"
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

/** @deprecated */
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
        "fixed z-9999 p-1.5",
        "box-border rounded-md border border-border bg-popover shadow-md"
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

export enum EContextDropdownMenuItemType {
  MENU_ITEM = "menu_item",
  SEPARATOR = "separator",
  MENU_GROUP = "menu_group",
  MENU_SUB = "menu_sub",
  LABEL = "label",
}

export interface IContextDropdownMenuItemBase {
  _type: EContextDropdownMenuItemType;
  _id: string;
  icon?: React.ReactNode;
  shortcut?: string | React.ReactNode;
}

export interface IContextDropdownMenuItem
  extends IContextDropdownMenuItemBase,
    React.ComponentPropsWithoutRef<typeof DropdownMenuPrimitive.Item> {
  _type: EContextDropdownMenuItemType.MENU_ITEM;
  inset?: boolean;
}

export interface IContextDropdownMenuItemSeparator
  extends IContextDropdownMenuItemBase,
    React.ComponentPropsWithoutRef<typeof DropdownMenuPrimitive.Separator> {
  _type: EContextDropdownMenuItemType.SEPARATOR;
}

export interface IContextDropdownMenuItemLabel
  extends IContextDropdownMenuItemBase,
    React.ComponentPropsWithoutRef<typeof DropdownMenuPrimitive.Label> {
  _type: EContextDropdownMenuItemType.LABEL;
}

export interface IContextDropdownMenuItemGroup
  extends IContextDropdownMenuItemBase,
    React.ComponentPropsWithoutRef<typeof DropdownMenuPrimitive.Group> {
  _type: EContextDropdownMenuItemType.MENU_GROUP;
  children: React.ReactNode;
}

export interface IContextDropdownMenuItemSub
  extends IContextDropdownMenuItemBase,
    React.ComponentPropsWithoutRef<typeof DropdownMenuPrimitive.Sub> {
  _type: EContextDropdownMenuItemType.MENU_SUB;
  label?: string | React.ReactNode;
  triggerProps?: React.ComponentPropsWithoutRef<
    typeof DropdownMenuPrimitive.SubTrigger
  > & {
    inset?: boolean;
  };
  contentProps?: React.ComponentPropsWithoutRef<
    typeof DropdownMenuPrimitive.SubContent
  >;
  items: (
    | IContextDropdownMenuItem
    | IContextDropdownMenuItemSeparator
    | IContextDropdownMenuItemLabel
    | IContextDropdownMenuItemGroup
    | IContextDropdownMenuItemSub
  )[];
}

export type ContextDropdownMenuItem =
  | IContextDropdownMenuItem
  | IContextDropdownMenuItemSeparator
  | IContextDropdownMenuItemLabel
  | IContextDropdownMenuItemGroup
  | IContextDropdownMenuItemSub;
export const ContextDropdownMenuItem = (props: {
  item: ContextDropdownMenuItem;
}) => {
  const { item } = props;

  if (item._type === EContextDropdownMenuItemType.MENU_ITEM) {
    const { children, icon = null, shortcut, ...rest } = item;
    return (
      <DropdownMenuItem {...rest}>
        {icon}
        {children}
        {shortcut && <DropdownMenuShortcut>{shortcut}</DropdownMenuShortcut>}
      </DropdownMenuItem>
    );
  }
  if (item._type === EContextDropdownMenuItemType.SEPARATOR) {
    return <DropdownMenuSeparator {...item} />;
  }
  if (item._type === EContextDropdownMenuItemType.LABEL) {
    return <DropdownMenuLabel {...item} />;
  }
  if (item._type === EContextDropdownMenuItemType.MENU_GROUP) {
    const { children, ...rest } = item;
    return <DropdownMenuGroup {...rest}>{children}</DropdownMenuGroup>;
  }
  if (item._type === EContextDropdownMenuItemType.MENU_SUB) {
    const { label, items, triggerProps, contentProps, ...rest } = item;
    return (
      <DropdownMenuSub {...rest}>
        <DropdownMenuSubTrigger {...triggerProps}>
          {label}
        </DropdownMenuSubTrigger>
        <DropdownMenuPortal>
          <DropdownMenuSubContent {...contentProps}>
            {items.map((item) => (
              <ContextDropdownMenuItem key={item._id} item={item} />
            ))}
          </DropdownMenuSubContent>
        </DropdownMenuPortal>
      </DropdownMenuSub>
    );
  }

  return null;
};
