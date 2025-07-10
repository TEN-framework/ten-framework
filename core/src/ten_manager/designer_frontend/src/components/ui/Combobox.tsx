//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
/* eslint-disable max-len */

import {
  Check,
  ChevronsUpDown,
  CirclePlus,
  LoaderCircleIcon,
} from "lucide-react";
import * as React from "react";
import { useTranslation } from "react-i18next";

import { Button, buttonVariants } from "@/components/ui/Button";
import {
  Command,
  CommandEmpty,
  CommandGroup,
  CommandInput,
  CommandItem,
  CommandList,
} from "@/components/ui/Command";
import {
  Popover,
  PopoverContent,
  PopoverTrigger,
} from "@/components/ui/Popover";
import { cn } from "@/lib/utils";

export type ComboboxOptions = {
  value: string;
  label: string;
};

interface ComboboxProps {
  options: ComboboxOptions[];
  selected: ComboboxOptions["value"];
  className?: string;
  placeholder?: string;
  disalbed?: boolean;
  onChange: (option: ComboboxOptions) => void;
  onCreate?: (label: ComboboxOptions["label"]) => void;
  disabled?: boolean;
  isLoading?: boolean;
  commandLabels?: {
    placeholder?: string;
    noItems?: string;
    enterValueToCreate?: string;
    noMatchedItems?: string;
  };
}

function CommandAddItem({
  query,
  onCreate,
}: {
  query: string;
  onCreate: () => void;
}) {
  const { t } = useTranslation();

  return (
    <div
      tabIndex={0}
      onClick={onCreate}
      onKeyDown={(event: React.KeyboardEvent<HTMLDivElement>) => {
        if (event.key === "Enter") {
          onCreate();
        }
      }}
      className={cn(
        "flex w-full cursor-pointer items-center gap-2 rounded-sm px-2 py-1.5 text-blue-500 text-sm focus:outline-none",
        "focus:!bg-blue-200 hover:bg-blue-200"
      )}
    >
      <CirclePlus className="mr-2 size-4" />
      {t("components.combobox.create", {
        query,
      })}
    </div>
  );
}

export function Combobox({
  options,
  selected,
  className,
  placeholder,
  disalbed,
  onChange,
  onCreate,
  isLoading,
  commandLabels,
}: ComboboxProps) {
  const [open, setOpen] = React.useState(false);
  const [query, setQuery] = React.useState("");
  const [canCreate, setCanCreate] = React.useState(true);
  const { t } = useTranslation();

  React.useEffect(() => {
    // Cannot create a new query if it is empty or has already been created
    // Unlike search, case sensitive here.
    const isAlreadyCreated = !options.some((option) => option.label === query);
    setCanCreate(!!(query && isAlreadyCreated));
  }, [query, options]);

  function handleSelect(option: ComboboxOptions) {
    if (onChange) {
      onChange(option);
      setOpen(false);
      setQuery("");
    }
  }

  function handleCreate() {
    if (onCreate && query) {
      onCreate(query);
      setOpen(false);
      setQuery("");
    }
  }

  if (isLoading) {
    return (
      <div
        className={cn(
          buttonVariants({ variant: "outline" }),
          "w-full font-normal"
        )}
      >
        <LoaderCircleIcon className="size-4 animate-spin" />
      </div>
    );
  }

  return (
    <Popover open={open} onOpenChange={setOpen}>
      <PopoverTrigger asChild>
        <Button
          type="button"
          variant="outline"
          role="combobox"
          disabled={disalbed || isLoading}
          aria-expanded={open}
          className={cn("w-full font-normal", className)}
        >
          {selected && selected.length > 0 ? (
            <div className="mr-auto truncate">
              {options.find((item) => item.value === selected)?.label}
            </div>
          ) : (
            <div className="mr-auto text-slate-600">
              {placeholder ?? "Select"}
            </div>
          )}
          <ChevronsUpDown className="ml-2 h-4 w-4 shrink-0 opacity-50" />
        </Button>
      </PopoverTrigger>
      <PopoverContent className="w-full min-w-[500px] p-0">
        <Command
          filter={(value, search) => {
            const v = value.toLocaleLowerCase();
            const s = search.toLocaleLowerCase();
            if (v.includes(s)) return 1;
            return 0;
          }}
        >
          <CommandInput
            placeholder={
              commandLabels?.placeholder || t("components.combobox.placeholder")
            }
            value={query}
            onValueChange={(value: string) => setQuery(value)}
            onKeyDown={(event: React.KeyboardEvent<HTMLInputElement>) => {
              if (event.key === "Enter") {
                // Avoid selecting what is displayed as a choice even if you press Enter for the conversion
                // Note that if you do this, you can select a choice with the arrow keys and press Enter, but it will not be selected
                event.preventDefault();
              }
            }}
          />
          <CommandEmpty className="flex w-full py-1 pl-1">
            {query && (
              <div className="space-y-1 py-1.5 pl-8 text-sm">
                <p>
                  {commandLabels?.noMatchedItems ||
                    t("components.combobox.noItems")}
                </p>
              </div>
            )}
            {query && onCreate && (
              <CommandAddItem query={query} onCreate={() => handleCreate()} />
            )}
          </CommandEmpty>

          <CommandList>
            <CommandGroup className="overflow-y-auto">
              {/* No options and no query */}
              {/* Even if written as a Child of CommandEmpty, it may not be displayed only the first time, so write it in CommandGroup. */}
              {options.length === 0 && !query && (
                <div className="space-y-1 py-1.5 pl-8 text-sm">
                  <p>
                    {commandLabels?.noItems || t("components.combobox.noItems")}
                  </p>
                  {onCreate && (
                    <p>
                      {commandLabels?.enterValueToCreate ||
                        t("components.combobox.enterValueToCreate")}
                    </p>
                  )}
                </div>
              )}

              {/* Create */}
              {canCreate && onCreate && (
                <CommandAddItem query={query} onCreate={() => handleCreate()} />
              )}

              {/* Select */}
              {options.map((option) => (
                <CommandItem
                  key={option.label}
                  tabIndex={0}
                  value={option.label}
                  onSelect={() => {
                    handleSelect(option);
                  }}
                  onKeyDown={(event: React.KeyboardEvent<HTMLDivElement>) => {
                    if (event.key === "Enter") {
                      // Process to prevent onSelect from firing, but it does not work well with StackBlitz.
                      event.stopPropagation();

                      handleSelect(option);
                    }
                  }}
                  className={cn(
                    "cursor-pointer",
                    // Override CommandItem class name
                    "focus:!bg-blue-200 hover:!bg-blue-200 aria-selected:bg-transparent"
                  )}
                >
                  {/* min to avoid the check icon being too small when the option.label is long. */}
                  <Check
                    className={cn(
                      "mr-2 size-4",
                      selected === option.value ? "opacity-100" : "opacity-0"
                    )}
                  />
                  {option.label}
                </CommandItem>
              ))}
            </CommandGroup>
          </CommandList>
        </Command>
      </PopoverContent>
    </Popover>
  );
}
