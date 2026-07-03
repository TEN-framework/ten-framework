//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { useTranslation } from "react-i18next";

import { ExtensionWidget } from "@/components/widget/extension-widget";
// eslint-disable-next-line max-len
import { ExtensionStoreWidget } from "@/components/widget/extension-widget/extension-store";
import { ETenPackageType } from "@/types/extension";
import type { IExtensionWidgetData, IWidget } from "@/types/widgets";

export const ExtensionStorePopupTitle = () => {
  const { t } = useTranslation();
  return t("extensionStore.title");
};

// eslint-disable-next-line @typescript-eslint/no-unused-vars
export const ExtensionStorePopupContent = (_props: { widget: IWidget }) => {
  return <ExtensionStoreWidget />;
};

export const ExtensionPopupTitle = (props: {
  name: string;
  type?: ETenPackageType;
}) => {
  const { name, type } = props;
  const { t } = useTranslation();
  return t("extensionStore.extensionTitle", {
    name,
    type: String(type || ETenPackageType.Extension).toUpperCase(),
  });
};

export const ExtensionPopupContent = (props: { widget: IWidget }) => {
  const { widget } = props;
  const metadata = widget.metadata as IExtensionWidgetData;

  return <ExtensionWidget {...metadata} />;
};
