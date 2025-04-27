//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
/* eslint-disable max-len */
import * as React from "react";

import { GraphSelectPopupContent } from "@/components/Popup/Default/GraphSelect";
import { AboutWidgetContent } from "@/components/Popup/Default/About";
import { PreferencesWidgetContent } from "@/components/Popup/Default/Preferences";
import {
  AppFolderPopupContent,
  AppCreatePopupContent,
  LoadedAppsPopupContent,
  AppRunPopupContent,
} from "@/components/Popup/Default/App";
import { ExtensionStorePopupContent } from "@/components/Popup/Default/Extension";
import { DocRefPopupContent } from "@/components/Popup/Default/DocRef";
import { EDefaultWidgetType, IDefaultWidget } from "@/types/widgets";

const PopupTabContentDefaultMappings: Record<
  EDefaultWidgetType,
  React.ComponentType<{ widget: IDefaultWidget }>
> = {
  [EDefaultWidgetType.About]: AboutWidgetContent,
  [EDefaultWidgetType.Preferences]: PreferencesWidgetContent,
  [EDefaultWidgetType.GraphSelect]: GraphSelectPopupContent,
  [EDefaultWidgetType.AppFolder]: AppFolderPopupContent,
  [EDefaultWidgetType.AppCreate]: AppCreatePopupContent,
  [EDefaultWidgetType.AppsManager]: LoadedAppsPopupContent,
  [EDefaultWidgetType.AppRun]: AppRunPopupContent,
  [EDefaultWidgetType.ExtensionStore]: ExtensionStorePopupContent,
  [EDefaultWidgetType.DocRef]: DocRefPopupContent,
};

export const PopupTabContentDefault = (props: { widget: IDefaultWidget }) => {
  const { widget } = props;

  const Renderer = PopupTabContentDefaultMappings[widget.metadata.type];

  if (!Renderer) return null;

  return (
    <Renderer
      key={`PopupTabContentDefault-${widget.widget_id}`}
      widget={widget}
    />
  );
};
