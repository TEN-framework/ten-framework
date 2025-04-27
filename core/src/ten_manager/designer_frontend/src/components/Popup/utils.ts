//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { EWidgetDisplayType, type IWidget } from "@/types/widgets";

export type TGroupedWidgets<T> = Record<string, Record<string, T[]>>;

export const groupWidgetsById = (
  widgets: IWidget[]
): TGroupedWidgets<IWidget> => {
  const popupWidgets = widgets.filter(
    (widget) => widget.display_type === EWidgetDisplayType.Popup
  );

  return popupWidgets.reduce(
    (prev: TGroupedWidgets<IWidget>, curr: IWidget) => {
      const { container_id, group_id } = curr;
      if (!prev[container_id]) {
        prev[container_id] = {};
      }
      if (!prev[container_id][group_id]) {
        prev[container_id][group_id] = [];
      }
      prev[container_id][group_id].push(curr);
      return prev;
    },
    {} as TGroupedWidgets<IWidget>
  );
};
