//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as React from "react";
import {
  FolderClosedIcon,
  FileIcon,
  FolderOpenIcon,
  ArrowDownUpIcon,
} from "lucide-react";
import { useTranslation } from "react-i18next";

import { Button } from "@/components/ui/Button";
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuLabel,
  DropdownMenuRadioGroup,
  DropdownMenuRadioItem,
  DropdownMenuSeparator,
  DropdownMenuTrigger,
} from "@/components/ui/DropdownMenu";
import { Input } from "@/components/ui/Input";
import { SpinnerLoading } from "@/components/Status/Loading";
import { cn } from "@/lib/utils";
import type { IFMItem } from "@/components/FileManager/utils";
import {
  EFMItemType,
  calculateDirDepth,
  baseDirEntriesToIFMItems,
  fmItemsToFMArray,
} from "@/components/FileManager/utils";
import { useRetrieveDirList } from "@/api/services/fileSystem";
import { useAppStore } from "@/store/app";
import type { TBaseDirEntry } from "@/types/fileSystem";

const DEFAULT_SELECTED_PATH = "/";

enum EFMItemSelectedStatus {
  UNSELECTED = "unselected",
  SELECTED = "selected",
  SELECTED_PARENT = "selected-parent",
}

function FileManagerColumn(props: {
  className?: string;
  children?: React.ReactNode;
  isLoading?: boolean;
  style?: React.CSSProperties;
}) {
  const { className, children, isLoading, style } = props;

  return (
    <ul
      className={cn(
        "px-2 overflow-y-auto",
        {
          "flex items-center justify-center text-gray-500 dark:text-gray-400":
            isLoading,
        },
        className
      )}
      style={style}
    >
      {isLoading ? <SpinnerLoading /> : children}
    </ul>
  );
}

function FileManagerColumnItem(props: {
  data: IFMItem;
  onClick?: () => void;
  className?: string;
  selectStatus?: "unselected" | "selected" | "selected-parent";
  disabled?: boolean;
}) {
  const {
    data,
    onClick,
    className,
    selectStatus = "unselected",
    disabled,
  } = props;

  const { t } = useTranslation();

  return (
    <li key={data.path}>
      <Button
        variant="ghost"
        size="sm"
        className={cn(
          "w-full justify-start",
          "hover:bg-gray-200 dark:hover:bg-gray-900",
          {
            "bg-gray-300 dark:bg-gray-800": selectStatus === "selected",
            "bg-gray-200 dark:bg-gray-900": selectStatus === "selected-parent",
          },
          className
        )}
        onClick={onClick}
        disabled={disabled}
      >
        {data.type === EFMItemType.FOLDER &&
          (selectStatus === "selected" ||
            selectStatus === "selected-parent") && (
            <FolderOpenIcon className="" />
          )}
        {data.type === EFMItemType.FOLDER && selectStatus === "unselected" && (
          <FolderClosedIcon className="" />
        )}
        {data.type !== EFMItemType.FOLDER && <FileIcon className="" />}
        <span className="overflow-hidden text-ellipsis whitespace-nowrap">
          {data.name}{" "}
          {selectStatus === "selected" ? `(${t("action.selected")})` : ""}
        </span>
      </Button>
    </li>
  );
}

export function FileManager(props: {
  allowSelectTypes?: EFMItemType[];
  onSelect?: (path: string) => void;
  data?: IFMItem[][];
  selectedPath?: string;
  className?: string;
  isLoading?: boolean;
  colWidth?: number;
  disableInput?: boolean;
}) {
  const {
    onSelect,
    data,
    selectedPath = DEFAULT_SELECTED_PATH,
    allowSelectTypes,
    className,
    isLoading,
    colWidth,
    disableInput = false,
  } = props;

  const [sortType, setSortType] = React.useState<"default" | "asc" | "desc">(
    "default"
  );
  const [mode, setMode] = React.useState<"select" | "input">("select");
  const [inputPathDepth, setInputPathDepth] = React.useState<number>(0);

  const handleInputChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    if (mode === "select") {
      setMode("input");
    }
    const rawValue = e.target.value;
    const value = rawValue.trim() || DEFAULT_SELECTED_PATH;
    setInputPathDepth(calculateDirDepth(value));
    onSelect?.(value);
  };

  const { t } = useTranslation();

  const colsEndEleRef = React.useRef<HTMLDivElement>(null);

  const colsMemo = React.useMemo(() => {
    const selectedPathDepth = calculateDirDepth(selectedPath);
    const currentPathDepth = Math.max(1, selectedPathDepth + 1);
    const levelData = data?.slice(1, currentPathDepth + 1) || [];
    const filteredData = levelData
      .reverse()
      .reduce(
        (
          prev: (IFMItem & {
            selectedStatus?: EFMItemSelectedStatus;
          })[][],
          cur: IFMItem[],
          curIdx: number
        ): (IFMItem & {
          selectedStatus?: EFMItemSelectedStatus;
        })[][] => {
          if (curIdx === 0) {
            const filteredItems = cur
              .filter((item) => item.path.startsWith(selectedPath))
              .map((item) => ({
                ...item,
                selectedStatus: EFMItemSelectedStatus.UNSELECTED,
              }));
            prev.push(filteredItems);
            return prev;
          }
          const selectedPathParentList = selectedPath
            .split("/")
            .slice(0, -curIdx)
            .join("/");
          const selectedPathCurrentList = selectedPath
            .split("/")
            .slice(0, -curIdx + 1)
            .join("/");
          const filteredItems = cur
            .filter((item) => item.path.startsWith(selectedPathParentList))
            .map((item) => ({
              ...item,
              selectedStatus: EFMItemSelectedStatus.UNSELECTED,
              selectedPathParentList,
              selectedPathCurrentList,
            }));
          if (curIdx === 1) {
            const selectedItem = filteredItems.find(
              (item) => item.path === selectedPath
            );
            if (selectedItem) {
              selectedItem.selectedStatus = EFMItemSelectedStatus.SELECTED;
            }
          } else {
            const selectedParentItem = filteredItems.find(
              (item) => item.path === selectedPathCurrentList
            );
            if (selectedParentItem) {
              selectedParentItem.selectedStatus =
                EFMItemSelectedStatus.SELECTED_PARENT;
            }
          }
          prev.push(filteredItems);
          return prev;
        },
        [] as (IFMItem & {
          selectedStatus?: EFMItemSelectedStatus;
        })[][]
      )
      .reverse();
    return filteredData;
  }, [data, selectedPath]);

  React.useEffect(() => {
    if (colsEndEleRef.current) {
      colsEndEleRef.current.scrollIntoView({ behavior: "smooth" });
    }
  }, [colsMemo?.length]);

  return (
    <div className={cn("w-full h-full space-y-2", className)}>
      <div className="flex items-center gap-2 justify-between">
        <Input
          className="h-10"
          value={selectedPath}
          onChange={handleInputChange}
          disabled={disableInput}
        />
        <DropdownMenu>
          <DropdownMenuTrigger asChild>
            <Button variant="ghost" size="icon">
              <ArrowDownUpIcon className="w-4 h-4" />
              <span className="sr-only">{t("extensionStore.filter.sort")}</span>
            </Button>
          </DropdownMenuTrigger>
          <DropdownMenuContent className="w-56">
            <DropdownMenuLabel>
              {t("extensionStore.filter.sort")}
            </DropdownMenuLabel>
            <DropdownMenuSeparator />
            <DropdownMenuRadioGroup
              value={sortType}
              onValueChange={setSortType as (value: string) => void}
            >
              <DropdownMenuRadioItem value="default">
                {t("extensionStore.filter.sort-default")}
              </DropdownMenuRadioItem>
              <DropdownMenuRadioItem value="asc">
                {t("extensionStore.filter.sort-name")}
              </DropdownMenuRadioItem>
              <DropdownMenuRadioItem value="desc">
                {t("extensionStore.filter.sort-name-desc")}
              </DropdownMenuRadioItem>
            </DropdownMenuRadioGroup>
          </DropdownMenuContent>
        </DropdownMenu>
      </div>
      <div
        className={cn(
          "flex py-2 w-full h-[calc(100%-3rem)]",
          "bg-gray-50 dark:bg-gray-900 rounded-lg",
          "overflow-x-auto"
        )}
      >
        <div className="flex w-fit">
          {(mode === "select"
            ? colsMemo.reduce(
                (prev, cur) => {
                  if (prev.length === 0 && cur.length === 0) {
                    return prev;
                  }
                  prev.push(cur);
                  return prev;
                },
                [] as (IFMItem & {
                  selectedStatus?: EFMItemSelectedStatus;
                })[][]
              )
            : colsMemo.slice(inputPathDepth > 0 ? inputPathDepth : 0)
          )?.map((item, idx) => (
            <FileManagerColumn
              key={idx}
              className="border-r border-gray-300"
              style={{ width: colWidth }}
              isLoading={isLoading && idx === colsMemo.length - 1}
            >
              {item
                .sort((a, b) => {
                  if (sortType === "default") return 0;
                  if (sortType === "asc") return a.name.localeCompare(b.name);
                  return b.name.localeCompare(a.name);
                })
                .map((item) => (
                  <FileManagerColumnItem
                    key={item.path}
                    data={item}
                    onClick={() => onSelect?.(item.path)}
                    selectStatus={item.selectedStatus}
                    disabled={
                      allowSelectTypes
                        ? !allowSelectTypes.includes(item.type)
                        : false
                    }
                  />
                ))}
            </FileManagerColumn>
          ))}
        </div>
        <div ref={colsEndEleRef} />
      </div>
    </div>
  );
}

export const AppFileManager = (props: {
  className?: string;
  isSaveLoading?: boolean;
  onSave?: (path?: string) => void;
  onCancel?: () => void;
  onReset?: () => void;
}) => {
  const { isSaveLoading, onSave, onCancel, onReset, className } = props;
  const [fmId, setFmId] = React.useState<string>(`fm-${Date.now()}`);

  const { t } = useTranslation();
  const { folderPath, setFolderPath, fmItems, setFmItems } = useAppStore();
  const {
    data,
    error,
    isLoading,
    mutate: mutateDirList,
  } = useRetrieveDirList(folderPath);

  const handleReset = () => {
    setFolderPath("/");
    mutateDirList();
    onReset?.();
    setFmId(`fm-${Date.now()}`);
  };

  const updateFmItems = (newFmItems: TBaseDirEntry[]) => {
    const currentFmItems = baseDirEntriesToIFMItems(newFmItems);
    const fmArray = fmItemsToFMArray(currentFmItems, fmItems);
    setFmItems(fmArray);
  };

  const handleSave = (path?: string) => {
    onSave?.(path);
    setFmItems([]);
  };

  React.useEffect(() => {
    if (error) {
      // toast.error(t("popup.default.errorGetBaseDir"));
      console.error(error);
      setFmItems([]);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [error]);

  React.useEffect(() => {
    if (!data?.entries) {
      return;
    }
    updateFmItems(data.entries);
    // Suppress the warning about the dependency array.
    // <fmItems> should not be a dependency.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [data, folderPath]);

  return (
    <div className={cn("flex flex-col gap-2 w-full h-full", className)}>
      <FileManager
        key={fmId}
        data={fmItems}
        allowSelectTypes={[EFMItemType.FOLDER]}
        className="w-full h-[calc(100%-3rem)]"
        onSelect={(path) => setFolderPath(path)}
        selectedPath={folderPath}
        isLoading={isLoading}
        colWidth={200}
      />
      <div className="flex justify-end h-fit gap-2">
        <Button
          variant="destructive"
          onClick={handleReset}
          disabled={isSaveLoading}
          className="mr-auto"
        >
          {t("action.reset")}
        </Button>
        {onCancel && (
          <Button variant="outline" onClick={onCancel} disabled={isSaveLoading}>
            {t("action.cancel")}
          </Button>
        )}
        {onSave && (
          <Button
            onClick={() => handleSave(folderPath)}
            disabled={isSaveLoading || !folderPath.trim()}
          >
            <>
              {isSaveLoading && <SpinnerLoading className="w-4 h-4 mr-2" />}
              {t("action.ok")}
            </>
          </Button>
        )}
      </div>
    </div>
  );
};
