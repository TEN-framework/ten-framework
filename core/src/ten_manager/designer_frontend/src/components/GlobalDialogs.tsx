//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as React from "react";
import { useTranslation } from "react-i18next";

import { Button } from "@/components/ui/Button";
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogFooter,
  DialogHeader,
  DialogTitle,
} from "@/components/ui/Dialog";
import { SpinnerLoading } from "@/components/Status/Loading";
import { type IDialog, useDialogStore } from "@/store/dialog";

export function GlobalDialogs() {
  const { dialogs } = useDialogStore();

  return (
    <>
      {dialogs.map((dialog) => (
        <GlobalDialog key={dialog.id} dialog={dialog} />
      ))}
    </>
  );
}

function GlobalDialog(props: { dialog: IDialog }) {
  const { dialog } = props;
  const [isLoading, setIsLoading] = React.useState(false);

  const { t } = useTranslation();

  return (
    <>
      <Dialog open={true}>
        <DialogContent>
          <DialogHeader>
            <DialogTitle>{dialog.title}</DialogTitle>
            {dialog?.subTitle && (
              <DialogDescription>{dialog.subTitle}</DialogDescription>
            )}
          </DialogHeader>
          <div className="max-h-[80dvh] overflow-y-auto px-0.5">
            {dialog.content}
          </div>
          {(dialog.onCancel || dialog.onConfirm) && (
            <DialogFooter>
              {dialog.onCancel && (
                <Button
                  variant="secondary"
                  onClick={async () => {
                    await dialog.onCancel?.();
                    await dialog.postCancel?.();
                  }}
                  disabled={isLoading}
                >
                  {dialog.cancelLabel || t("action.cancel")}
                </Button>
              )}
              {dialog.onConfirm && (
                <Button
                  variant={dialog.variant || "default"}
                  disabled={isLoading}
                  onClick={async () => {
                    setIsLoading(true);
                    await dialog.onConfirm?.();
                    await dialog.postConfirm?.();
                    setIsLoading(false);
                  }}
                >
                  {isLoading && <SpinnerLoading className="w-4 h-4" />}
                  {dialog.confirmLabel || t("action.confirm")}
                </Button>
              )}
            </DialogFooter>
          )}
        </DialogContent>
      </Dialog>
    </>
  );
}
