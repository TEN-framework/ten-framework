//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
/* eslint-disable max-len */
import type * as React from "react";
import { AboutWidgetContent } from "@/components/popup/default/about";
import {
  AppCreatePopupContent,
  AppFolderPopupContent,
  AppRunPopupContent,
  LoadedAppsPopupContent,
} from "@/components/popup/default/app";
import { DocRefPopupContent } from "@/components/popup/default/doc-ref";
import { ExtensionStorePopupContent } from "@/components/popup/default/extension";
import { PreferencesWidgetContent } from "@/components/popup/default/preferences";
import { RTCWidgetContent } from "@/components/popup/default/rtc";
import { TrulienceConfigWidgetContent } from "@/components/popup/default/trulience-config";
import { EDefaultWidgetType, type IDefaultWidget } from "@/types/widgets";

const PopupTabContentDefaultMappings: Record<
  EDefaultWidgetType,
  React.ComponentType<{ widget: IDefaultWidget }>
> = {
  [EDefaultWidgetType.About]: AboutWidgetContent,
  [EDefaultWidgetType.Preferences]: PreferencesWidgetContent,
  [EDefaultWidgetType.AppFolder]: AppFolderPopupContent,
  [EDefaultWidgetType.AppCreate]: AppCreatePopupContent,
  [EDefaultWidgetType.AppsManager]: LoadedAppsPopupContent,
  [EDefaultWidgetType.AppRun]: AppRunPopupContent,
  [EDefaultWidgetType.ExtensionStore]: ExtensionStorePopupContent,
  [EDefaultWidgetType.DocRef]: DocRefPopupContent,
  [EDefaultWidgetType.RTCInteraction]: RTCWidgetContent,
  [EDefaultWidgetType.TrulienceConfig]: TrulienceConfigWidgetContent,
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
