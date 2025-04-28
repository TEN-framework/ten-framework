//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { useTranslation } from "react-i18next";

import {
  ExtensionStoreWidget,
  ExtensionWidget,
} from "@/components/Widget/ExtensionWidget";
import { IExtensionWidgetData, IWidget } from "@/types/widgets";

export const ExtensionStorePopupTitle = () => {
  const { t } = useTranslation();
  return t("extensionStore.title");
};

// eslint-disable-next-line @typescript-eslint/no-unused-vars
export const ExtensionStorePopupContent = (_props: { widget: IWidget }) => {
  return <ExtensionStoreWidget />;
};

export const ExtensionPopupTitle = (props: { name: string }) => {
  const { name } = props;
  const { t } = useTranslation();
  return t("extensionStore.extensionTitle", { name });
};

export const ExtensionPopupContent = (props: { widget: IWidget }) => {
  const { widget } = props;
  const { versions, name } = widget.metadata as IExtensionWidgetData;

  return <ExtensionWidget versions={versions} name={name} />;
};
