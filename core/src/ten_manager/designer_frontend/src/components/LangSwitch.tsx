//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { useTranslation } from "react-i18next";
import { LanguagesIcon, CheckIcon } from "lucide-react";

import { Button, ButtonProps } from "@/components/ui/Button";
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuTrigger,
} from "@/components/ui/DropdownMenu";
import { cn } from "@/lib/utils";
import { EPreferencesLocale } from "@/types/apps";

export function LanguageToggle(props: { buttonProps?: ButtonProps }) {
  const {
    buttonProps: {
      className: btnClassName,
      children: btnChildren,
      ...restButtonProps
    } = {},
  } = props;
  const { i18n, t } = useTranslation();

  return (
    <DropdownMenu>
      <DropdownMenuTrigger asChild>
        <Button
          variant="ghost"
          size="icon"
          className={cn("bg-transparent", btnClassName)}
          {...restButtonProps}
        >
          <LanguagesIcon className="h-[1.2rem] w-[1.2rem]" />
          <span className={cn("sr-only")}>{t("header.language.title")}</span>
          {btnChildren}
        </Button>
      </DropdownMenuTrigger>
      <DropdownMenuContent align="end">
        <DropdownMenuItem
          onClick={() => i18n.changeLanguage(EPreferencesLocale.EN_US)}
        >
          <span>{t("header.language.enUS")}</span>
          {i18n.language === EPreferencesLocale.EN_US && (
            <CheckIcon className="ml-auto h-4 w-4" />
          )}
        </DropdownMenuItem>
        <DropdownMenuItem
          onClick={() => i18n.changeLanguage(EPreferencesLocale.ZH_CN)}
        >
          <span>{t("header.language.zhCN")}</span>
          {i18n.language === EPreferencesLocale.ZH_CN && (
            <CheckIcon className="ml-auto h-4 w-4" />
          )}
        </DropdownMenuItem>{" "}
        <DropdownMenuItem
          onClick={() => i18n.changeLanguage(EPreferencesLocale.ZH_TW)}
        >
          <span>{t("header.language.zhTW")}</span>
          {i18n.language === EPreferencesLocale.ZH_TW && (
            <CheckIcon className="ml-auto h-4 w-4" />
          )}
        </DropdownMenuItem>
        <DropdownMenuItem
          onClick={() => i18n.changeLanguage(EPreferencesLocale.JA_JP)}
        >
          <span>{t("header.language.jaJP")}</span>
          {i18n.language === EPreferencesLocale.JA_JP && (
            <CheckIcon className="ml-auto h-4 w-4" />
          )}
        </DropdownMenuItem>
      </DropdownMenuContent>
    </DropdownMenu>
  );
}
