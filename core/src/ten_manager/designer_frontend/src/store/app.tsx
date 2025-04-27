//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { create } from "zustand";
import { devtools } from "zustand/middleware";
import { z } from "zod";

import { TEN_DEFAULT_APP_RUN_SCRIPT } from "@/constants";

import { type IFMItem } from "@/components/FileManager/utils";
import {
  type IExtensionAddon,
  PREFERENCES_SCHEMA,
  EPreferencesLocale,
  IApp,
} from "@/types/apps";
import { type IGraph } from "@/types/graphs";

export interface IAppStore {
  currentWorkspace: {
    initialized?: boolean;
    graph: IGraph | null;
    app: IApp | null;
  };
  updateCurrentWorkspace: (currentWorkspace: {
    graph?: IGraph | null;
    app?: IApp | null;
  }) => void;
  /** @deprecated */
  runScript: string;
  /** @deprecated */
  setRunScript: (runScript: string) => void;
  folderPath: string;
  setFolderPath: (folderPath: string) => void;
  fmItems: IFMItem[][];
  setFmItems: (fmItems: IFMItem[][]) => void;
  addons: IExtensionAddon[];
  setAddons: (addons: IExtensionAddon[]) => void;
  defaultOsArch: {
    os?: string;
    arch?: string;
  };
  setDefaultOsArch: (osArch: { os?: string; arch?: string }) => void;
  preferences: z.infer<typeof PREFERENCES_SCHEMA>;
  setPreferences: (
    key: keyof z.infer<typeof PREFERENCES_SCHEMA>,
    value: Partial<
      z.infer<typeof PREFERENCES_SCHEMA>[keyof z.infer<
        typeof PREFERENCES_SCHEMA
      >]
    >
  ) => void;
}

export const useAppStore = create<IAppStore>()(
  devtools((set) => ({
    currentWorkspace: {
      graph: null,
      app: null,
      initialized: false,
    },
    updateCurrentWorkspace: (currentWorkspace: {
      graph?: IGraph | null;
      app?: IApp | null;
    }) =>
      set((state) => ({
        currentWorkspace: {
          ...state.currentWorkspace,
          graph:
            currentWorkspace.graph !== undefined
              ? currentWorkspace.graph
              : state.currentWorkspace.graph,
          app:
            currentWorkspace.app !== undefined
              ? currentWorkspace.app
              : state.currentWorkspace.app,
          initialized: true,
        },
      })),
    runScript: TEN_DEFAULT_APP_RUN_SCRIPT,
    setRunScript: (runScript: string) => set({ runScript }),
    folderPath: "/",
    setFolderPath: (folderPath: string) => set({ folderPath }),
    fmItems: [[]],
    setFmItems: (fmItems: IFMItem[][]) => set({ fmItems }),
    addons: [],
    setAddons: (addons: IExtensionAddon[]) => set({ addons }),
    defaultOsArch: {
      os: undefined,
      arch: undefined,
    },
    setDefaultOsArch: (osArch: { os?: string; arch?: string }) =>
      set({ defaultOsArch: osArch }),
    preferences: {
      logviewer_line_size: 1000,
      locale: EPreferencesLocale.EN_US, // TODO: get from the backend
    },
    setPreferences: (
      key: keyof z.infer<typeof PREFERENCES_SCHEMA>,
      value: Partial<
        z.infer<typeof PREFERENCES_SCHEMA>[keyof z.infer<
          typeof PREFERENCES_SCHEMA
        >]
      >
    ) =>
      set((state) => ({
        preferences: {
          ...state.preferences,
          [key]: value,
        },
      })),
  }))
);
