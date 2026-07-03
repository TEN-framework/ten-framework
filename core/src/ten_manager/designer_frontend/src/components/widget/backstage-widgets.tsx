//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as React from "react";

// eslint-disable-next-line max-len
import { LogViewerBackstageWidget } from "@/components/widget/log-viewer-widget";
import { useWidgetStore } from "@/store/widget";
import { EWidgetCategory } from "@/types/widgets";

export function BackstageWidgets() {
  const { backstageWidgets } = useWidgetStore();

  const [logViewerWidgets] = React.useMemo(() => {
    const logViewerWidgets = backstageWidgets.filter(
      (widget) => widget.category === EWidgetCategory.LogViewer
    );
    return [logViewerWidgets];
  }, [backstageWidgets]);

  return (
    <>
      {logViewerWidgets.map((widget) => (
        <LogViewerBackstageWidget key={widget.widget_id} {...widget} />
      ))}
    </>
  );
}
