//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import {
  ArrowDownAZIcon,
  ArrowDownZAIcon,
  ArrowUpDownIcon,
  FilterIcon,
  XIcon,
} from "lucide-react";
import { useTranslation } from "react-i18next";

import { Button } from "@/components/ui/Button";
import {
  DropdownMenu,
  DropdownMenuCheckboxItem,
  DropdownMenuContent,
  DropdownMenuPortal,
  DropdownMenuRadioGroup,
  DropdownMenuRadioItem,
  DropdownMenuSeparator,
  DropdownMenuSub,
  DropdownMenuSubContent,
  DropdownMenuSubTrigger,
  DropdownMenuTrigger,
} from "@/components/ui/DropdownMenu";
import { Input } from "@/components/ui/Input";
import { cn } from "@/lib/utils";
import { useWidgetStore } from "@/store/widget";

export const ExtensionSearch = (props: { className?: string }) => {
  const { className } = props;

  const { extSearch, setExtSearch, extFilter, updateExtFilter } =
    useWidgetStore();
  const { t } = useTranslation();

  return (
    <div className={cn("flex items-center justify-between gap-2", className)}>
      <Input
        placeholder={t("extensionStore.searchPlaceholder")}
        value={extSearch}
        onChange={(e) => setExtSearch(e.target.value || "")}
        className="w-full border-none shadow-none focus-visible:ring-0"
      />
      <div className="flex items-center">
        {extSearch.trim() !== "" && (
          <XIcon
            className="mr-2 size-3 cursor-pointer text-secondary-foreground"
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
              <FilterIcon className="h-4 w-4" />
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
                      <ArrowUpDownIcon className="mr-2 size-4" />
                      <span>{t("extensionStore.filter.sort-default")}</span>
                    </DropdownMenuRadioItem>
                    <DropdownMenuRadioItem value="name">
                      <ArrowDownAZIcon className="mr-2 size-4" />
                      <span>{t("extensionStore.filter.sort-name")}</span>
                    </DropdownMenuRadioItem>
                    <DropdownMenuRadioItem value="name-desc">
                      <ArrowDownZAIcon className="mr-2 size-4" />
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
