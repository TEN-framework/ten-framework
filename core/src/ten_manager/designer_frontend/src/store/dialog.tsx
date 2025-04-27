//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as React from "react";
import { create } from "zustand";
import { devtools } from "zustand/middleware";

export interface IDialog {
  id: string;
  title: string;
  subTitle?: string;
  content: string | React.ReactNode;
  onConfirm?: () => Promise<void>;
  onCancel?: () => Promise<void>;
  postConfirm?: () => Promise<void>;
  postCancel?: () => Promise<void>;
  cancelLabel?: string | React.ReactNode;
  confirmLabel?: string | React.ReactNode;
  variant?: "default" | "destructive";
}

export const useDialogStore = create<{
  dialogs: IDialog[];
  appendDialog: (
    dialog: IDialog,
    opts?: {
      force?: boolean;
    }
  ) => void;
  removeDialog: (dialogId: string) => void;
}>()(
  devtools((set) => ({
    dialogs: [],
    appendDialog: (dialog: IDialog, opts?: { force?: boolean }) =>
      set((state) => {
        const exists = state.dialogs.some((d) => d.id === dialog.id);
        if (exists) {
          if (opts?.force) {
            return { dialogs: [...state.dialogs, dialog] };
          }
          return state;
        }
        return { dialogs: [...state.dialogs, dialog] };
      }),
    removeDialog: (dialogId: string) =>
      set((state) => ({
        dialogs: state.dialogs.filter((d) => d.id !== dialogId),
      })),
  }))
);
