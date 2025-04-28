//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as React from "react";
import { useTranslation } from "react-i18next";

import EditorWidget from "@/components/Widget/EditorWidget";
import { useWidgetStore } from "@/store/widget";

import type { IEditorWidget, IEditorWidgetRef } from "@/types/widgets";

export const EditorPopupTitle = (props: {
  title: string;
  widgetId: string;
}) => {
  const { title, widgetId } = props;

  const { widgets } = useWidgetStore();

  const targetWidget = widgets.find(
    (widget) => widget.widget_id === widgetId
  ) as IEditorWidget;

  const isEditing = targetWidget?.metadata?.isContentChanged;

  const { t } = useTranslation();

  return (
    <div className="flex items-center gap-1.5">
      <span className="font-medium text-foreground/90 font-sans">{title}</span>
      <span className="text-foreground/50 text-sm font-sans">
        {isEditing ? `(${t("action.unsaved")})` : ""}
      </span>
    </div>
  );
};

export const EditorPopupContent = (props: { widget: IEditorWidget }) => {
  const { widget } = props;
  const { refs, ...rest } = widget.metadata;
  const ref = React.useRef<IEditorWidgetRef>(null);

  if (refs && widget.widget_id) {
    refs[widget.widget_id] = ref;
  }

  return <EditorWidget id={widget.widget_id} data={rest} ref={ref} />;
};
