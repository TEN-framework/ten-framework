//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import {
  ArrowUpIcon,
  SquareArrowOutUpRightIcon,
  ChevronsRightIcon,
} from "lucide-react";
import { useTranslation, Trans } from "react-i18next";

import { BadgeWithRef, Badge } from "@/components/ui/Badge";
import {
  Tooltip,
  TooltipContent,
  TooltipProvider,
  TooltipTrigger,
} from "@/components/ui/Tooltip";
import { useVersion, useCheckUpdate } from "@/api/services/common";
import { cn } from "@/lib/utils";
import { TEN_FRAMEWORK_RELEASE_URL } from "@/constants";

export function Version() {
  const { version } = useVersion();
  const { data: updateData } = useCheckUpdate();

  const { t } = useTranslation();

  return (
    <TooltipProvider delayDuration={100}>
      <Tooltip>
        <TooltipTrigger asChild>
          <BadgeWithRef variant="secondary" className="relative gap-2">
            <span className="uppercase">{t("ten")}</span>
            <a
              href={updateData?.release_page || TEN_FRAMEWORK_RELEASE_URL}
              target="_blank"
              rel="noopener noreferrer"
              className="inline-flex items-center gap-1"
            >
              <span className="uppercase">{version}</span>
              {updateData?.update_available && (
                <ArrowUpIcon
                  className={cn(
                    "size-3 stroke-3",
                    "text-emerald-500 animate-bounce"
                  )}
                />
              )}
            </a>
          </BadgeWithRef>
        </TooltipTrigger>
        <TooltipContent className="max-w-md">
          {updateData?.update_available ? (
            <>
              <p className="font-bold text-lg">
                {t("header.newVersionAvailable")}
              </p>
              <div className="flex items-center gap-2 select-none">
                <Badge variant="secondary">
                  <span className="uppercase">{version}</span>
                  <span className="text-xs font-normal italic">
                    ({t("header.currentVersion")})
                  </span>
                </Badge>
                <ChevronsRightIcon className="size-4" />
                <a
                  href={updateData?.release_page || TEN_FRAMEWORK_RELEASE_URL}
                  target="_blank"
                  rel="noopener noreferrer"
                >
                  <Badge variant="secondary">
                    <span className="uppercase">
                      {updateData?.latest_version}
                    </span>
                    <span className="text-xs font-normal italic">
                      ({t("header.latestVersion")})
                    </span>
                  </Badge>
                </a>
              </div>
              {updateData?.message && (
                <p className="my-1">{updateData?.message}</p>
              )}
              <p className="my-1">
                <Trans
                  t={t}
                  i18nKey="header.newVersionAvailableDescription"
                  components={[
                    <TooltipContentLink
                      href={TEN_FRAMEWORK_RELEASE_URL}
                    ></TooltipContentLink>,
                  ]}
                />
              </p>
            </>
          ) : (
            <>
              <p className="font-bold text-lg">{t("header.currentIsLatest")}</p>
              {updateData?.message && (
                <p className="my-1">{updateData?.message}</p>
              )}
              <p className="my-1">
                <Trans
                  t={t}
                  i18nKey="header.currentIsLatestDescription"
                  components={[
                    <TooltipContentLink href={TEN_FRAMEWORK_RELEASE_URL} />,
                  ]}
                />
              </p>
            </>
          )}
        </TooltipContent>
      </Tooltip>
    </TooltipProvider>
  );
}

const TooltipContentLink = ({
  href,
  children,
}: {
  href: string;
  children?: React.ReactNode;
}) => {
  return (
    <a
      href={href}
      target="_blank"
      rel="noopener noreferrer"
      className="inline-flex items-center gap-1 underline"
    >
      {children}
      <SquareArrowOutUpRightIcon className="size-3" />
    </a>
  );
};
