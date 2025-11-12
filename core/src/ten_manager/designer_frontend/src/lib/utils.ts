//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { type ClassValue, clsx } from "clsx";
import { twMerge } from "tailwind-merge";

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs));
}

export function formatNumberWithCommas(number: number) {
  return number.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",");
}

/**
 * Compare two version strings including pre-release versions
 * @param a such as "1.0.0" or "1.0.0-beta1"
 * @param b such as "1.0.1" or "1.0.0-rc2"
 * @returns 1 if a > b, -1 if a < b, 0 if a == b
 */
export function compareVersions(a: string, b: string): number {
  const [aVersion, aPrerelease = ""] = a.split("-");
  const [bVersion, bPrerelease = ""] = b.split("-");

  const aParts = aVersion.split(".").map(Number);
  const bParts = bVersion.split(".").map(Number);

  // Compare version numbers
  for (let i = 0; i < Math.max(aParts.length, bParts.length); i++) {
    const aPart = aParts[i] || 0;
    const bPart = bParts[i] || 0;
    if (aPart > bPart) return 1;
    if (aPart < bPart) return -1;
  }

  // If versions are equal, compare pre-release
  if (!aPrerelease && !bPrerelease) return 0;
  if (!aPrerelease) return 1; // release > pre-release
  if (!bPrerelease) return -1; // pre-release < release

  const preReleaseOrder = { alpha: 1, beta: 2, rc: 3 };
  const aType = aPrerelease.match(/^(alpha|beta|rc)/)?.[1] || "";
  const bType = bPrerelease.match(/^(alpha|beta|rc)/)?.[1] || "";
  const aNum = parseInt(aPrerelease.match(/\d+$/)?.[0] || "0", 10);
  const bNum = parseInt(bPrerelease.match(/\d+$/)?.[0] || "0", 10);

  if (aType !== bType) {
    return (
      (preReleaseOrder[aType as keyof typeof preReleaseOrder] || 0) -
      (preReleaseOrder[bType as keyof typeof preReleaseOrder] || 0)
    );
  }
  return aNum - bNum;
}

export const calcAbbreviatedBaseDir = (baseDir: string) => {
  const parts = baseDir.split("/");
  if (parts.length <= 2) {
    return baseDir;
  }
  return `${parts[0]}/${parts[1]}/.../${parts[parts.length - 1]}`;
};
