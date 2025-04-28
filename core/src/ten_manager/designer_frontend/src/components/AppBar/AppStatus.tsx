//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { SaveIcon, FilePenLineIcon, SaveOffIcon } from "lucide-react";

import { Button } from "@/components/ui/Button";
import { cn } from "@/lib/utils";

// eslint-disable-next-line react-refresh/only-export-components
export enum EAppStatus {
  SAVED = "Saved",
  UNSAVED = "Unsaved",
  EDITING = "Editing",
}

export function AppStatus(props: { className?: string; status?: EAppStatus }) {
  const { className, status } = props;
  if (!status) {
    return null;
  }
  return (
    <div className={cn("flex items-center gap-2", "w-fit", className)}>
      <div className="flex items-center">
        {status === EAppStatus.SAVED && <SaveIcon className="w-4 h-4 me-2" />}
        {status === EAppStatus.UNSAVED && (
          <SaveOffIcon className="w-4 h-4 me-2" />
        )}
        {status === EAppStatus.EDITING && (
          <FilePenLineIcon className="w-4 h-4 me-2" />
        )}
        {status}
      </div>
      {status === EAppStatus.EDITING && (
        <div className="flex items-center text-primary">
          <Button variant="outline" size="sm" className="">
            <SaveIcon className="w-3 h-3" />
            <span className="text-xs">Save</span>
          </Button>
        </div>
      )}
    </div>
  );
}
