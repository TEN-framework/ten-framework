//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { Trans, useTranslation } from "react-i18next";

import { Separator } from "@/components/ui/separator";
import { TEN_FRAMEWORK_GITHUB_URL, TEN_FRAMEWORK_URL } from "@/constants";
import { cn } from "@/lib/utils";
import type { IWidget } from "@/types/widgets";

export const AboutWidgetTitle = () => {
  const { t } = useTranslation();

  return t("header.menuDesigner.about");
};

// eslint-disable-next-line @typescript-eslint/no-unused-vars
export const AboutWidgetContent = (_props: { widget: IWidget }) => {
  const { t } = useTranslation();

  return (
    <div className="flex h-full w-full flex-col gap-2">
      <p
        className={cn(
          "text-center",
          "text-base italic",
          "font-['Segoe_UI',Tahoma,Geneva,Verdana,sans-serif]"
        )}
      >
        <Trans
          components={[<PoweredByTenFramework key="tenframework" />]}
          t={t}
          i18nKey="header.poweredByTenFramework"
        />
      </p>
      <Separator className="" />
      <ul className="flex flex-col gap-2">
        <li className="flex items-center justify-between gap-2">
          <span>{t("header.officialSite")}</span>
          <a
            href={TEN_FRAMEWORK_URL}
            target="_blank"
            rel="noopener noreferrer"
            className="text-blue-600 underline"
          >
            {TEN_FRAMEWORK_URL}
          </a>
        </li>
        <li className="flex items-center justify-between gap-2">
          <span>{t("header.github")}</span>
          <a
            href={TEN_FRAMEWORK_GITHUB_URL}
            target="_blank"
            rel="noopener noreferrer"
            className="text-blue-600 underline"
          >
            {TEN_FRAMEWORK_GITHUB_URL}
          </a>
        </li>
      </ul>
    </div>
  );
};

export function PoweredByTenFramework(props: {
  className?: string;
  children?: React.ReactNode;
}) {
  const { t } = useTranslation();
  return (
    <span className={cn("font-bold text-foreground", props.className)}>
      {t("tenFramework")}
    </span>
  );
}
