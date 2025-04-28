//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { clsx, type ClassValue } from "clsx";
import { twMerge } from "tailwind-merge";

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs));
}

export function formatNumberWithCommas(number: number) {
  return number.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",");
}

/**
 * Compare two version strings
 * @param a such as "1.0.0"
 * @param b such as "1.0.1"
 * @returns 1 if a > b, -1 if a < b, 0 if a == b
 */
export function compareVersions(a: string, b: string) {
  const aParts = a.split(".");
  const bParts = b.split(".");
  for (let i = 0; i < aParts.length; i++) {
    const aPart = aParts[i];
    const bPart = bParts[i];
    if (aPart > bPart) {
      return 1;
    } else if (aPart < bPart) {
      return -1;
    }
  }
  return 0;
}
