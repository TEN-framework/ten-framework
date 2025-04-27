//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { Moon, Sun } from "lucide-react";
import { useTranslation } from "react-i18next";

import { Button, ButtonProps } from "@/components/ui/Button";
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuTrigger,
} from "@/components/ui/DropdownMenu";
import { useTheme } from "@/components/use-theme";
import { cn } from "@/lib/utils";

export function ModeToggle(props: {
  buttonProps?: ButtonProps;
  hideIcon?: boolean;
}) {
  const {
    buttonProps: {
      className: btnClassName,
      children: btnChildren,
      ...restButtonProps
    } = {},
    hideIcon = false,
  } = props;
  const { setTheme } = useTheme();
  const { t } = useTranslation();
  return (
    <DropdownMenu>
      <DropdownMenuTrigger asChild>
        <Button
          variant="ghost"
          size="icon"
          className={cn("bg-transparent", btnClassName)}
          {...restButtonProps}
        >
          {!hideIcon && (
            <>
              <Sun
                className={cn(
                  "h-[1.2rem] w-[1.2rem]",
                  "rotate-0 scale-100 transition-all",
                  "dark:-rotate-90 dark:scale-0"
                )}
              />
              <Moon
                className={cn(
                  "h-[1.2rem] w-[1.2rem]",
                  "absolute rotate-90 scale-0 transition-all",
                  "dark:rotate-0 dark:scale-100"
                )}
              />
            </>
          )}
          <span className="sr-only">{t("header.theme.title")}</span>
          {btnChildren}
        </Button>
      </DropdownMenuTrigger>
      <DropdownMenuContent align="end">
        <DropdownMenuItem onClick={() => setTheme("light")}>
          {t("header.theme.light")}
        </DropdownMenuItem>
        <DropdownMenuItem onClick={() => setTheme("dark")}>
          {t("header.theme.dark")}
        </DropdownMenuItem>
        <DropdownMenuItem onClick={() => setTheme("system")}>
          {t("header.theme.system")}
        </DropdownMenuItem>
      </DropdownMenuContent>
    </DropdownMenu>
  );
}
