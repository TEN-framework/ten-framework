//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
/* eslint-disable react-refresh/only-export-components */

import { ChevronRightIcon } from "lucide-react";
import type React from "react";
import { Button, type ButtonProps } from "@/components/ui/button";
import {
  ContextMenuCheckboxItem,
  ContextMenuItem,
  ContextMenuLabel,
  ContextMenuRadioGroup,
  ContextMenuRadioItem,
  ContextMenuSeparator,
  ContextMenuShortcut,
  ContextMenuSub,
  ContextMenuSubContent,
  ContextMenuSubTrigger,
} from "@/components/ui/context-menu";
import { Separator } from "@/components/ui/separator";
import {
  Tooltip,
  TooltipContent,
  TooltipProvider,
  TooltipTrigger,
} from "@/components/ui/tooltip";
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
        {...props}
        className={cn(
          "flex w-full justify-start whitespace-nowrap px-2.5 py-1.5",
          "h-auto cursor-pointer font-normal",
          props.className
        )}
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
                // biome-ignore lint/suspicious/noArrayIndexKey: <ignore>
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
    >
      {items.map((item, index) => (
        // biome-ignore lint/suspicious/noArrayIndexKey: <ignore>
        <ContextItem key={index} {...item} />
      ))}
    </div>
  );
};

export default ContextMenuContainer;

export enum ERightClickContextMenuItemType {
  MENU_ITEM = "menu_item",
  SEPARATOR = "separator",
  MENU_GROUP = "menu_group",
  MENU_SUB = "menu_sub",
  LABEL = "label",
  CHECKBOX_ITEM = "checkbox_item",
  RADIO_GROUP = "radio_group",
  RADIO_ITEM = "radio_item",
}

export interface IRightClickContextMenuItemBase {
  _type: ERightClickContextMenuItemType;
  _id: string;
  icon?: React.ReactNode;
  shortcut?: string | React.ReactNode;
}

export interface IRightClickContextMenuItem
  extends IRightClickContextMenuItemBase,
    React.ComponentPropsWithoutRef<typeof ContextMenuItem> {
  _type: ERightClickContextMenuItemType.MENU_ITEM;
  inset?: boolean;
}

export interface IRightClickContextMenuItemSeparator
  extends IRightClickContextMenuItemBase,
    React.ComponentPropsWithoutRef<typeof ContextMenuSeparator> {
  _type: ERightClickContextMenuItemType.SEPARATOR;
}

export interface IRightClickContextMenuItemLabel
  extends IRightClickContextMenuItemBase,
    React.ComponentPropsWithoutRef<typeof ContextMenuLabel> {
  _type: ERightClickContextMenuItemType.LABEL;
}

export interface IRightClickContextMenuItemCheckbox
  extends IRightClickContextMenuItemBase,
    React.ComponentPropsWithoutRef<typeof ContextMenuCheckboxItem> {
  _type: ERightClickContextMenuItemType.CHECKBOX_ITEM;
}

export interface IRightClickContextMenuItemRadioGroup
  extends IRightClickContextMenuItemBase,
    React.ComponentPropsWithoutRef<typeof ContextMenuRadioGroup> {
  _type: ERightClickContextMenuItemType.RADIO_GROUP;
  children: React.ReactNode;
}

export interface IRightClickContextMenuItemRadio
  extends IRightClickContextMenuItemBase,
    React.ComponentPropsWithoutRef<typeof ContextMenuRadioItem> {
  _type: ERightClickContextMenuItemType.RADIO_ITEM;
}

export interface IRightClickContextMenuItemSub
  extends IRightClickContextMenuItemBase,
    React.ComponentPropsWithoutRef<typeof ContextMenuSub> {
  _type: ERightClickContextMenuItemType.MENU_SUB;
  label?: string | React.ReactNode;
  triggerProps?: React.ComponentPropsWithoutRef<
    typeof ContextMenuSubTrigger
  > & {
    inset?: boolean;
  };
  contentProps?: React.ComponentPropsWithoutRef<typeof ContextMenuSubContent>;
  items: (
    | IRightClickContextMenuItem
    | IRightClickContextMenuItemSeparator
    | IRightClickContextMenuItemLabel
    | IRightClickContextMenuItemCheckbox
    | IRightClickContextMenuItemRadioGroup
    | IRightClickContextMenuItemRadio
    | IRightClickContextMenuItemSub
  )[];
}

export type RightClickContextMenuItem =
  | IRightClickContextMenuItem
  | IRightClickContextMenuItemSeparator
  | IRightClickContextMenuItemLabel
  | IRightClickContextMenuItemCheckbox
  | IRightClickContextMenuItemRadioGroup
  | IRightClickContextMenuItemRadio
  | IRightClickContextMenuItemSub;

export const RightClickContextMenuItem = (props: {
  item: RightClickContextMenuItem;
}) => {
  const { item } = props;

  if (item._type === ERightClickContextMenuItemType.MENU_ITEM) {
    const { children, icon = null, shortcut, ...rest } = item;
    return (
      <ContextMenuItem {...rest}>
        {icon}
        {children}
        {shortcut && <ContextMenuShortcut>{shortcut}</ContextMenuShortcut>}
      </ContextMenuItem>
    );
  }
  if (item._type === ERightClickContextMenuItemType.SEPARATOR) {
    return <ContextMenuSeparator {...item} />;
  }
  if (item._type === ERightClickContextMenuItemType.LABEL) {
    return <ContextMenuLabel {...item} />;
  }
  if (item._type === ERightClickContextMenuItemType.CHECKBOX_ITEM) {
    const { children, icon = null, shortcut, ...rest } = item;
    return (
      <ContextMenuCheckboxItem {...rest}>
        {icon}
        {children}
        {shortcut && <ContextMenuShortcut>{shortcut}</ContextMenuShortcut>}
      </ContextMenuCheckboxItem>
    );
  }
  if (item._type === ERightClickContextMenuItemType.RADIO_GROUP) {
    const { children, ...rest } = item;
    return <ContextMenuRadioGroup {...rest}>{children}</ContextMenuRadioGroup>;
  }
  if (item._type === ERightClickContextMenuItemType.RADIO_ITEM) {
    const { children, icon = null, shortcut, ...rest } = item;
    return (
      <ContextMenuRadioItem {...rest}>
        {icon}
        {children}
        {shortcut && <ContextMenuShortcut>{shortcut}</ContextMenuShortcut>}
      </ContextMenuRadioItem>
    );
  }
  if (item._type === ERightClickContextMenuItemType.MENU_SUB) {
    const { label, items, triggerProps, contentProps, ...rest } = item;
    return (
      <ContextMenuSub {...rest}>
        <ContextMenuSubTrigger {...triggerProps}>{label}</ContextMenuSubTrigger>
        <ContextMenuSubContent {...contentProps}>
          {items.map((item) => (
            <RightClickContextMenuItem key={item._id} item={item} />
          ))}
        </ContextMenuSubContent>
      </ContextMenuSub>
    );
  }

  return null;
};
