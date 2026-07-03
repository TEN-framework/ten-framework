//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import { fieldConfig, ZodProvider } from "@autoform/zod";
import { useTranslation } from "react-i18next";
import { z } from "zod";
import { AutoForm } from "@/components/ui/autoform";
import { Button } from "@/components/ui/button";
import { useAppStore, useWidgetStore } from "@/store";
import type { IWidget } from "@/types/widgets";

export const TrulienceConfigWidgetTitle = () => {
  const { t } = useTranslation();

  return t("trulienceConfig.title");
};

export const TrulienceConfigWidgetContent = (_props: { widget: IWidget }) => {
  const { widget } = _props;
  const { t } = useTranslation();
  const { removeWidget } = useWidgetStore();
  const { setPreferences, preferences } = useAppStore();
  const schema: [string, z.ZodType][] = [
    [
      "enabled",
      z
        .boolean()
        .optional()
        .superRefine(
          fieldConfig({
            label: t("trulienceConfig.enabled"),
          })
        ),
    ],
    [
      "trulienceAvatarId",
      z
        .string()
        .optional()
        .superRefine(
          fieldConfig({
            label: t("trulienceConfig.trulienceAvatarId"),
          })
        ),
    ],
    [
      "trulienceAvatarToken",
      z
        .string()
        .optional()
        .superRefine(
          fieldConfig({
            label: t("trulienceConfig.trulienceAvatarToken"),
          })
        ),
    ],
    [
      "trulienceSdkUrl",
      z
        .string()
        .optional()
        .superRefine(
          fieldConfig({
            label: t("trulienceConfig.trulienceSdkUrl"),
          })
        ),
    ],
    [
      "trulienceAnimationURL",
      z
        .string()
        .optional()
        .superRefine(
          fieldConfig({
            label: t("trulienceConfig.trulienceAnimationURL"),
          })
        ),
    ],
  ];

  return (
    <div className="flex h-full w-full flex-col gap-2 overflow-y-auto">
      <AutoForm
        onSubmit={async (data) => {
          setPreferences("trulience", {
            ...preferences.trulience,
            ...data,
          });
          removeWidget(widget.widget_id);
        }}
        defaultValues={{
          enabled: false,
          trulienceAvatarId: undefined,
          trulienceAvatarToken: undefined,
          trulienceSdkUrl: "https://trulience.com/sdk/trulience.sdk.js",
          trulienceAnimationURL: "https://trulience.com",
        }}
        values={{
          ...preferences.trulience,
        }}
        schema={new ZodProvider(z.object(Object.fromEntries(schema)))}
      >
        <Button type="submit" className="w-full">
          {t("action.confirm")}
        </Button>
      </AutoForm>
    </div>
  );
};
