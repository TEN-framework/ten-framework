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
import { Button, buttonVariants } from "@/components/ui/button";
import { Checkbox } from "@/components/ui/checkbox";
import {
  Command,
  CommandEmpty,
  CommandGroup,
  CommandInput,
  CommandItem,
  CommandList,
} from "@/components/ui/command";
import {
  Popover,
  PopoverContent,
  PopoverTrigger,
} from "@/components/ui/popover";
import { cn } from "@/lib/utils";

const MAX_DISPLAY_ITEMS = 3;

export type ComboboxOptions = {
  value: string;
  label: string;
  selectable?: boolean;
};

type ComboboxCommandLabels = {
  placeholder?: string;
  noItems?: string;
  enterValueToCreate?: string;
  noMatchedItems?: string;
};

interface ComboboxBaseProps {
  options: ComboboxOptions[];
  className?: string;
  placeholder?: string;
  disabled?: boolean;
  onCreate?: (label: ComboboxOptions["label"]) => void;
  isLoading?: boolean;
  commandLabels?: ComboboxCommandLabels;
}

interface SingleComboboxProps extends ComboboxBaseProps {
  mode?: "single";
  selected?: ComboboxOptions["value"] | null;
  onChange: (option: ComboboxOptions) => void;
  maxSelectedItems?: never;
}

interface MultipleComboboxProps extends ComboboxBaseProps {
  mode: "multiple";
  selected: ComboboxOptions["value"][];
  onChange: (options: ComboboxOptions[]) => void;
  maxSelectedItems?: number;
}

type ComboboxProps = SingleComboboxProps | MultipleComboboxProps;

function CommandAddItem({
  query,
  onCreate,
}: {
  query: string;
  onCreate: () => void;
}) {
  const { t } = useTranslation();

  return (
    <button
      type="button"
      onClick={onCreate}
      onKeyDown={(event: React.KeyboardEvent<HTMLButtonElement>) => {
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
    </button>
  );
}

export interface MultiSelectorWithCheckboxProps
  extends Omit<MultipleComboboxProps, "mode"> {
  selectAllLabel?: string;
  searchPlaceholder?: string;
  emptyStateMessage?: string;
}

export function MultiSelectorWithCheckbox(
  props: MultiSelectorWithCheckboxProps
) {
  const {
    options,
    className,
    placeholder,
    disabled,
    onCreate,
    isLoading,
    commandLabels,
    selected,
    onChange,
    maxSelectedItems = MAX_DISPLAY_ITEMS,
    selectAllLabel,
    searchPlaceholder,
    emptyStateMessage,
  } = props;
  const { t } = useTranslation();
  const [open, setOpen] = React.useState(false);
  const [query, setQuery] = React.useState("");
  const [canCreate, setCanCreate] = React.useState(true);

  React.useEffect(() => {
    const isAlreadyCreated = !options.some((option) => option.label === query);
    setCanCreate(!!(query && isAlreadyCreated));
  }, [query, options]);

  const getOptionFromValue = React.useCallback(
    (value: string): ComboboxOptions =>
      options.find((item) => item.value === value) || {
        value,
        label: value,
        selectable: true,
      },
    [options]
  );

  const selectedValues = React.useMemo(
    () => Array.from(new Set(selected)),
    [selected]
  );

  const selectedSet = React.useMemo(
    () => new Set(selectedValues),
    [selectedValues]
  );

  const selectedOptionsForDisplay = React.useMemo(
    () => selectedValues.map((value) => getOptionFromValue(value)),
    [getOptionFromValue, selectedValues]
  );

  const multipleSelectionLabel = React.useMemo(() => {
    const displayedItems = selectedOptionsForDisplay.slice(0, maxSelectedItems);
    const truncatedLabel = displayedItems.map((item) => item.label).join(", ");
    const remainingCount =
      selectedOptionsForDisplay.length - displayedItems.length;

    if (remainingCount > 0) {
      return `${truncatedLabel} +${remainingCount}`;
    }

    return truncatedLabel;
  }, [maxSelectedItems, selectedOptionsForDisplay]);

  const normalizedQuery = React.useMemo(
    () => query.trim().toLowerCase(),
    [query]
  );

  const filteredOptions = React.useMemo(() => {
    if (!normalizedQuery) {
      return options;
    }

    return options.filter((option) =>
      option.value.toLowerCase().includes(normalizedQuery)
    );
  }, [options, normalizedQuery]);

  // Only consider selectable options for select all
  const filteredSelectableOptions = React.useMemo(
    () => filteredOptions.filter((option) => option.selectable !== false),
    [filteredOptions]
  );

  const allFilteredSelected =
    filteredSelectableOptions.length > 0 &&
    filteredSelectableOptions.every((option) => selectedSet.has(option.value));

  const partiallyFilteredSelected =
    filteredSelectableOptions.some((option) => selectedSet.has(option.value)) &&
    !allFilteredSelected;

  function updateSelection(nextSet: Set<string>) {
    const nextValues = Array.from(nextSet.values());
    onChange(nextValues.map((value) => getOptionFromValue(value)));
  }

  function handleToggle(option: ComboboxOptions) {
    if (option.selectable === false) return;
    const next = new Set(selectedSet);
    if (next.has(option.value)) {
      next.delete(option.value);
    } else {
      next.add(option.value);
    }
    updateSelection(next);
  }

  function handleSelectAll() {
    if (filteredSelectableOptions.length === 0) {
      return;
    }

    const next = new Set(selectedSet);
    if (allFilteredSelected) {
      filteredSelectableOptions.forEach((option) => {
        next.delete(option.value);
      });
    } else {
      filteredSelectableOptions.forEach((option) => {
        next.add(option.value);
      });
    }
    updateSelection(next);
  }

  function handleCreate() {
    if (onCreate && query) {
      onCreate(query);
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
          disabled={disabled || isLoading}
          aria-expanded={open}
          className={cn("w-full font-normal", className)}
        >
          {selectedOptionsForDisplay.length > 0 ? (
            <div className="mr-auto truncate">{multipleSelectionLabel}</div>
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
              searchPlaceholder ??
              commandLabels?.placeholder ??
              t("components.combobox.placeholder")
            }
            value={query}
            onValueChange={(value: string) => setQuery(value)}
            onKeyDown={(event: React.KeyboardEvent<HTMLInputElement>) => {
              if (event.key === "Enter") {
                event.preventDefault();
              }
            }}
          />
          <CommandEmpty className="flex w-full py-1 pl-1">
            {query && (
              <div className="space-y-1 py-1.5 pl-8 text-sm">
                <p>
                  {emptyStateMessage ||
                    commandLabels?.noMatchedItems ||
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

              {/* SELECT ALL must be at the top */}
              {filteredSelectableOptions.length > 0 && (
                <CommandItem
                  value={`${selectAllLabel ?? t("graph.select-all")} ${query}`}
                  onSelect={() => handleSelectAll()}
                  className={cn(
                    "cursor-pointer",
                    "focus:!bg-blue-200 hover:!bg-blue-200 aria-selected:bg-transparent"
                  )}
                >
                  <Checkbox
                    checked={
                      allFilteredSelected
                        ? true
                        : partiallyFilteredSelected
                          ? "indeterminate"
                          : false
                    }
                    onCheckedChange={() => handleSelectAll()}
                    onClick={(event) => event.stopPropagation()}
                    className="mr-2"
                  />
                  <span>{selectAllLabel ?? t("graph.select-all")}</span>
                </CommandItem>
              )}

              {canCreate && onCreate && (
                <CommandAddItem query={query} onCreate={() => handleCreate()} />
              )}

              {filteredOptions.map((option) => (
                <CommandItem
                  key={option.value}
                  tabIndex={0}
                  value={option.value}
                  onSelect={() => handleToggle(option)}
                  onKeyDown={(event: React.KeyboardEvent<HTMLDivElement>) => {
                    if (event.key === "Enter") {
                      event.stopPropagation();
                      handleToggle(option);
                    }
                  }}
                  className={cn(
                    "cursor-pointer",
                    "focus:!bg-blue-200 hover:!bg-blue-200 aria-selected:bg-transparent",
                    option.selectable === false &&
                      "pointer-events-none select-none opacity-50"
                  )}
                  aria-disabled={option.selectable === false}
                  disabled={option.selectable === false}
                >
                  <Checkbox
                    checked={selectedSet.has(option.value)}
                    onCheckedChange={() => handleToggle(option)}
                    onClick={(event) => event.stopPropagation()}
                    className="mr-2"
                    disabled={option.selectable === false}
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

export function Combobox(props: ComboboxProps) {
  const {
    options,
    className,
    placeholder,
    onCreate,
    isLoading,
    commandLabels,
    disabled,
    maxSelectedItems = MAX_DISPLAY_ITEMS,
  } = props;
  const isMultiple = props.mode === "multiple";
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

  const getOptionFromValue = React.useCallback(
    (value: string): ComboboxOptions =>
      options.find((item) => item.value === value) || {
        value,
        label: value,
        selectable: true,
      },
    [options]
  );

  const multipleSelected = isMultiple
    ? (props as MultipleComboboxProps).selected
    : undefined;
  const singleSelected = !isMultiple
    ? (props as SingleComboboxProps).selected
    : undefined;

  const selectedValues = React.useMemo(() => {
    if (Array.isArray(multipleSelected)) {
      return multipleSelected;
    }

    if (typeof singleSelected === "string" && singleSelected.length > 0) {
      return [singleSelected];
    }

    return [];
  }, [multipleSelected, singleSelected]);

  const selectedOptionsForDisplay = React.useMemo(
    () => selectedValues.map((value) => getOptionFromValue(value)),
    [getOptionFromValue, selectedValues]
  );

  const multipleSelectionLabel = React.useMemo(() => {
    if (!isMultiple) return null;

    const displayedItems = selectedOptionsForDisplay.slice(0, maxSelectedItems);
    const truncatedLabel = displayedItems.map((item) => item.label).join(", ");
    const remainingCount =
      selectedOptionsForDisplay.length - displayedItems.length;

    if (remainingCount > 0) {
      return `${truncatedLabel} +${remainingCount}`;
    }

    return truncatedLabel;
  }, [isMultiple, maxSelectedItems, selectedOptionsForDisplay]);

  function handleSelect(option: ComboboxOptions) {
    if (option.selectable === false) return;

    if (isMultiple) {
      const multipleProps = props as MultipleComboboxProps;
      const current = new Set(multipleProps.selected);

      if (current.has(option.value)) {
        current.delete(option.value);
      } else {
        current.add(option.value);
      }

      const nextValues = Array.from(current.values());
      multipleProps.onChange(
        nextValues.map((value) => getOptionFromValue(value))
      );
      return;
    }

    const singleProps = props as SingleComboboxProps;
    singleProps.onChange(option);
    setOpen(false);
    setQuery("");
  }

  function handleCreate() {
    if (onCreate && query) {
      onCreate(query);
      if (!isMultiple) {
        setOpen(false);
      }
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
          disabled={disabled || isLoading}
          aria-expanded={open}
          className={cn("w-full font-normal", className)}
        >
          {selectedOptionsForDisplay.length > 0 ? (
            <div className="mr-auto truncate">
              {isMultiple
                ? multipleSelectionLabel
                : selectedOptionsForDisplay[0]?.label}
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

              {/* Note: There is no select all for single-select Combobox, so nothing changes for it */}
              {/* Create */}
              {canCreate && onCreate && (
                <CommandAddItem query={query} onCreate={() => handleCreate()} />
              )}

              {/* Select */}
              {options.map((option) => (
                <CommandItem
                  key={option.value}
                  tabIndex={0}
                  value={option.value}
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
                    "focus:!bg-blue-200 hover:!bg-blue-200 aria-selected:bg-transparent",
                    option.selectable === false &&
                      "pointer-events-none select-none opacity-50"
                  )}
                  aria-disabled={option.selectable === false}
                  disabled={option.selectable === false}
                >
                  {/* min to avoid the check icon being too small when the option.label is long. */}
                  <Check
                    className={cn(
                      "mr-2 size-4",
                      selectedValues.includes(option.value)
                        ? "opacity-100"
                        : "opacity-0"
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
