//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as React from "react";

import {
  NavigationMenu,
  NavigationMenuList,
} from "@/components/ui/NavigationMenu";
import { ModeToggle } from "@/components/ModeToggle";
import { LanguageToggle } from "@/components/LangSwitch";
import { AppMenu } from "@/components/AppBar/Menu/AppMenu";
import { GraphMenu } from "@/components/AppBar/Menu/GraphMenu";
import { DesignerMenu } from "@/components/AppBar/Menu/DesignerMenu";
import { ExtensionMenu } from "@/components/AppBar/Menu/ExtensionMenu";
import { AppStatus } from "@/components/AppBar/AppStatus";
import { GHStargazersCount, GHTryTENAgent } from "@/components/Widget/GH";
import { Version } from "@/components/AppBar/Version";
import { cn } from "@/lib/utils";
import { TEN_FRAMEWORK_GH_OWNER, TEN_FRAMEWORK_GH_REPO } from "@/constants";

interface AppBarProps {
  onAutoLayout: () => void;
  className?: string;
}

export default function AppBar({ onAutoLayout, className }: AppBarProps) {
  // init disable state
  const [disableMenuClick, setDisableMenuClick] = React.useState(false);

  // init reference array
  const targetMenuListRef = React.useRef<HTMLButtonElement[]>([]);

  // Create observer on first render
  React.useEffect(() => {
    // Callback function
    const observerCallback = (mutationsList: MutationRecord[]) => {
      for (const mutation of mutationsList) {
        if (
          mutation.type === "attributes" &&
          mutation.attributeName === "data-state" &&
          (mutation.target as HTMLElement).dataset.state === "open"
        ) {
          setDisableMenuClick(true);
          const timeout = setTimeout(() => {
            setDisableMenuClick(false);
            clearTimeout(timeout);
          }, 1000);
        }
      }
    };

    // Init MutationObserver
    const observer = new MutationObserver(observerCallback);

    // Add ref nodes to observer watch
    targetMenuListRef.current.forEach((element) => {
      if (element) {
        observer.observe(element, {
          attributes: true,
        });
      }
    });

    // Disconnect on dismount
    return () => {
      observer.disconnect();
    };
  }, []);

  const onNavChange = () => {
    setTimeout(() => {
      const triggers = document.querySelectorAll(
        '.submenu-trigger[data-state="open"]'
      );
      if (triggers.length === 0) return;

      const firstTrigger = triggers[0] as HTMLElement;

      document.documentElement.style.setProperty(
        "--menu-left-position",
        `${firstTrigger.offsetLeft}px`
      );
    });
  };

  return (
    <header
      className={cn(
        "flex justify-between items-center  text-sm select-none",
        "h-10 w-full px-5",
        "fixed top-0 left-0 right-0",
        "bg-zinc-50 dark:bg-zinc-900",
        "border-b border-[#e5e7eb] dark:border-[#374151]",
        className
      )}
    >
      <NavigationMenu onValueChange={onNavChange}>
        <NavigationMenuList>
          <DesignerMenu
            disableMenuClick={disableMenuClick}
            idx={0}
            triggerListRef={targetMenuListRef}
          />
          <AppMenu
            disableMenuClick={disableMenuClick}
            idx={1}
            triggerListRef={targetMenuListRef}
          />
          <GraphMenu
            disableMenuClick={disableMenuClick}
            idx={2}
            triggerListRef={targetMenuListRef}
            onAutoLayout={onAutoLayout}
          />
          <ExtensionMenu
            disableMenuClick={disableMenuClick}
            idx={3}
            triggerListRef={targetMenuListRef}
          />
        </NavigationMenuList>
      </NavigationMenu>

      {/* Middle part is the status bar. */}
      <AppStatus
        className={cn(
          "flex-1 flex justify-center items-center",
          "text-xs text-muted-foreground"
        )}
      />

      {/* Right part is the logo. */}
      <div className="flex items-center gap-1.5">
        <LanguageToggle />
        <ModeToggle />
        <GHStargazersCount
          owner={TEN_FRAMEWORK_GH_OWNER}
          repo={TEN_FRAMEWORK_GH_REPO}
        />
        <GHTryTENAgent />
        <Version />
      </div>
    </header>
  );
}
