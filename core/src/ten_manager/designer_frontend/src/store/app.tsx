//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import type { z } from "zod";
import { create } from "zustand";
import { devtools } from "zustand/middleware";
import type { IFMItem } from "@/components/file-manager/utils";
import {
  EPreferencesLocale,
  type IApp,
  type PREFERENCES_SCHEMA,
} from "@/types/apps";
import type { IGraph } from "@/types/graphs";

export interface IAppStore {
  /** @deprecated */
  currentWorkspace: {
    initialized?: boolean;
    graph: IGraph | null;
    app: IApp | null;
  };
  /** @deprecated */
  updateCurrentWorkspace: (currentWorkspace: {
    graph?: IGraph | null;
    app?: IApp | null;
  }) => void;
  selectedGraphs: IGraph[] | undefined;
  setSelectedGraphs: (graphs: IGraph[]) => void;
  appendSelectedGraphs: (graph: IGraph[]) => void;
  removeSelectedGraphs: (graph: IGraph[]) => void;
  folderPath: string;
  setFolderPath: (folderPath: string) => void;
  fmItems: IFMItem[][];
  setFmItems: (fmItems: IFMItem[][]) => void;
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
      graph?: IGraph | null; // TODO: remove
      app?: IApp | null;
    }) =>
      set((state) => ({
        currentWorkspace: {
          ...state.currentWorkspace,
          // graph:
          //   currentWorkspace.graph !== undefined
          //     ? currentWorkspace.graph
          //     : state.currentWorkspace.graph,
          app:
            currentWorkspace.app !== undefined
              ? currentWorkspace.app
              : state.currentWorkspace.app,
          initialized: true,
        },
      })),
    appendSelectedGraphs: (graph: IGraph[]) =>
      set((state) => {
        const existing = state.selectedGraphs || [];
        const existingIds = new Set(existing.map((g) => g.uuid));
        const newGraphs = graph.filter((g) => !existingIds.has(g.uuid));
        return {
          selectedGraphs: [...existing, ...newGraphs],
        };
      }),
    removeSelectedGraphs: (graph: IGraph[]) =>
      set((state) => {
        const existing = state.selectedGraphs || [];
        const graphIdsToRemove = new Set(graph.map((g) => g.uuid));
        const newGraphs = existing.filter((g) => !graphIdsToRemove.has(g.uuid));
        return {
          selectedGraphs: newGraphs,
        };
      }),
    selectedGraphs: undefined,
    setSelectedGraphs: (graphs: IGraph[]) => set({ selectedGraphs: graphs }),
    folderPath: "/",
    setFolderPath: (folderPath: string) => set({ folderPath }),
    fmItems: [[]],
    setFmItems: (fmItems: IFMItem[][]) => set({ fmItems }),
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
