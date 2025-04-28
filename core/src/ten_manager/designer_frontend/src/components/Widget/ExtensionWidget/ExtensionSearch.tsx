//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import {
  FilterIcon,
  ArrowDownAZIcon,
  ArrowDownZAIcon,
  ArrowUpDownIcon,
  XIcon,
} from "lucide-react";
import { useTranslation } from "react-i18next";

import { Button } from "@/components/ui/Button";
import { Input } from "@/components/ui/Input";
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuPortal,
  DropdownMenuSeparator,
  DropdownMenuSub,
  DropdownMenuSubContent,
  DropdownMenuSubTrigger,
  DropdownMenuTrigger,
  DropdownMenuCheckboxItem,
  DropdownMenuRadioGroup,
  DropdownMenuRadioItem,
} from "@/components/ui/DropdownMenu";
import { cn } from "@/lib/utils";
import { useWidgetStore } from "@/store/widget";

export const ExtensionSearch = (props: { className?: string }) => {
  const { className } = props;

  const { extSearch, setExtSearch, extFilter, updateExtFilter } =
    useWidgetStore();
  const { t } = useTranslation();

  return (
    <div className={cn("flex items-center gap-2 justify-between", className)}>
      <Input
        placeholder={t("extensionStore.searchPlaceholder")}
        value={extSearch}
        onChange={(e) => setExtSearch(e.target.value || "")}
        className="w-full border-none shadow-none focus-visible:ring-0"
      />
      <div className="flex items-center">
        {extSearch.trim() !== "" && (
          <XIcon
            className="size-3 mr-2 text-secondary-foreground cursor-pointer"
            onClick={() => setExtSearch("")}
          />
        )}
        <DropdownMenu>
          <DropdownMenuTrigger asChild>
            <Button
              variant="ghost"
              size="icon"
              className="size-9 cursor-pointer rounded-none"
            >
              <FilterIcon className="w-4 h-4" />
            </Button>
          </DropdownMenuTrigger>
          <DropdownMenuContent className="w-56">
            <DropdownMenuCheckboxItem
              checked={extFilter.showUninstalled}
              onCheckedChange={(checked) =>
                updateExtFilter({ showUninstalled: checked })
              }
            >
              {t("extensionStore.filter.showUninstalled")}
            </DropdownMenuCheckboxItem>
            <DropdownMenuCheckboxItem
              checked={extFilter.showInstalled}
              onCheckedChange={(checked) =>
                updateExtFilter({ showInstalled: checked })
              }
            >
              {t("extensionStore.filter.showInstalled")}
            </DropdownMenuCheckboxItem>
            <DropdownMenuSeparator />
            <DropdownMenuSub>
              <DropdownMenuSubTrigger>
                <ArrowDownAZIcon />
                <span>{t("extensionStore.filter.sort")}</span>
              </DropdownMenuSubTrigger>
              <DropdownMenuPortal>
                <DropdownMenuSubContent>
                  <DropdownMenuRadioGroup
                    value={extFilter.sort}
                    onValueChange={(value) =>
                      updateExtFilter({
                        sort: value as "default" | "name" | "name-desc",
                      })
                    }
                  >
                    <DropdownMenuRadioItem value="default">
                      <ArrowUpDownIcon className="size-4 mr-2" />
                      <span>{t("extensionStore.filter.sort-default")}</span>
                    </DropdownMenuRadioItem>
                    <DropdownMenuRadioItem value="name">
                      <ArrowDownAZIcon className="size-4 mr-2" />
                      <span>{t("extensionStore.filter.sort-name")}</span>
                    </DropdownMenuRadioItem>
                    <DropdownMenuRadioItem value="name-desc">
                      <ArrowDownZAIcon className="size-4 mr-2" />
                      <span>{t("extensionStore.filter.sort-name-desc")}</span>
                    </DropdownMenuRadioItem>
                  </DropdownMenuRadioGroup>
                </DropdownMenuSubContent>
              </DropdownMenuPortal>
            </DropdownMenuSub>
          </DropdownMenuContent>
        </DropdownMenu>
      </div>
    </div>
  );
};
