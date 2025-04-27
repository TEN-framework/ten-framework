//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import type { TBaseDirEntry } from "@/types/fileSystem";

export enum EFMItemType {
  FILE = "file",
  FOLDER = "folder",
}

export interface IFMItem {
  name: string;
  path: string;
  type: EFMItemType;
  dirDepth: number;
}

export const calculateDirDepth = (path: string) => {
  return (path.match(/\//g) || []).length || 0;
};

export const baseDirEntriesToIFMItems = (data: TBaseDirEntry[]): IFMItem[] => {
  return data.map((entry) => ({
    name: entry.name,
    path: entry.path,
    type: entry.is_dir ? EFMItemType.FOLDER : EFMItemType.FILE,
    dirDepth: calculateDirDepth(entry.path),
  }));
};

export const fmItemsToFMArray = (
  data: IFMItem[],
  memory: IFMItem[][]
): IFMItem[][] => {
  // Create a new array to store the result
  const result = [...memory];

  // Process each item in data
  data.forEach((item) => {
    // Ensure array exists for this depth level
    while (result.length <= item.dirDepth) {
      result.push([]);
    }

    // Find existing item at this depth level
    const levelArray = result[item.dirDepth];
    const existingIndex = levelArray.findIndex(
      (existing) => existing.path === item.path
    );

    if (existingIndex >= 0) {
      // Update existing item
      levelArray[existingIndex] = {
        ...levelArray[existingIndex],
        ...item,
      };
    } else {
      // Add new item
      levelArray.push(item);
    }
  });

  return result;
};

/** @deprecated */
export const getSelectedPathAndNeighbors = (
  fmItems: IFMItem[][],
  selectedPath: string
) => {
  const selectedPathDepth = calculateDirDepth(selectedPath);
  const selectedPathParent = selectedPath.substring(
    0,
    selectedPath.lastIndexOf("/")
  );
  const selectedPathGrandParent = selectedPathParent.substring(
    0,
    selectedPathParent.lastIndexOf("/")
  );

  return {
    prev2: (fmItems[1] || []).filter((item) => item.dirDepth === 1),
    prev1: (fmItems[selectedPathDepth - 1] || []).filter((item) =>
      item.path.startsWith(selectedPathGrandParent)
    ),
    current: (fmItems[selectedPathDepth] || []).filter((item) =>
      item.path.startsWith(selectedPathParent)
    ),
    next1: (fmItems[selectedPathDepth + 1] || []).filter((item) =>
      item.path.startsWith(selectedPath)
    ),
    next2: [],
  };
};
