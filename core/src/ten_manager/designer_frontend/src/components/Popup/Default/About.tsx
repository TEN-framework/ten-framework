//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { useTranslation, Trans } from "react-i18next";

import { Separator } from "@/components/ui/Separator";
import { cn } from "@/lib/utils";
import { TEN_FRAMEWORK_URL, TEN_FRAMEWORK_GITHUB_URL } from "@/constants";
import { IWidget } from "@/types/widgets";

export const AboutWidgetTitle = () => {
  const { t } = useTranslation();

  return t("header.menuDesigner.about");
};

// eslint-disable-next-line @typescript-eslint/no-unused-vars
export const AboutWidgetContent = (_props: { widget: IWidget }) => {
  const { t } = useTranslation();

  return (
    <div className="flex flex-col gap-2 h-full w-full">
      <p
        className={cn(
          "text-center",
          "italic text-base",
          "font-['Segoe_UI',Tahoma,Geneva,Verdana,sans-serif]"
        )}
      >
        <Trans
          components={[<PoweredByTenFramework />]}
          t={t}
          i18nKey="header.poweredByTenFramework"
        />
      </p>
      <Separator className="" />
      <ul className="flex flex-col gap-2">
        <li className="flex justify-between items-center gap-2">
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
        <li className="flex justify-between items-center gap-2">
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
