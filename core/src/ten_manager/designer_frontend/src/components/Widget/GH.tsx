//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as React from "react";
import { StarIcon, BotIcon } from "lucide-react";
import { useTranslation } from "react-i18next";

import { badgeVariants } from "@/components/ui/Badge";
import {
  Tooltip,
  TooltipContent,
  TooltipProvider,
  TooltipTrigger,
} from "@/components/ui/Tooltip";
import { GHIcon } from "@/components/Icons";
import { SpinnerLoading } from "@/components/Status/Loading";
import { cn } from "@/lib/utils";
import { useGHRepository } from "@/api/services/github";
import { formatNumberWithCommas } from "@/lib/utils";
import { TEN_AGENT_GITHUB_URL, TEN_AGENT_URL } from "@/constants";
import { useHelpText } from "@/api/services/helpText";
import { EHelpTextKey } from "@/api/endpoints";
import { Separator } from "@/components/ui/Separator";

import { type VariantProps } from "class-variance-authority";

export function GHStargazersCount(props: {
  owner: string;
  repo: string;
  className?: string;
}) {
  const { owner, repo, className } = props;

  const { i18n } = useTranslation();
  const { repository, error, isLoading } = useGHRepository(owner, repo);
  const {
    data: helpText,
    error: helpTextError,
    isLoading: helpTextIsLoading,
  } = useHelpText(EHelpTextKey.TEN_FRAMEWORK, i18n.language);

  const shouldFallbackMemo = React.useMemo(() => {
    return isLoading || error || !repository?.stargazers_count;
  }, [isLoading, error, repository]);

  React.useEffect(() => {
    if (helpTextError) {
      console.error(helpTextError);
    } else if (error) {
      console.error(error);
    }
  }, [helpTextError, error]);

  return (
    <TooltipProvider delayDuration={100}>
      <Tooltip>
        <TooltipTrigger asChild>
          <BadgeLink
            href={`https://github.com/${owner}/${repo}`}
            target="_blank"
            rel="noopener noreferrer"
            className={cn(
              "flex items-center gap-1.5",
              "text-sm",
              badgeVariants({ variant: "secondary" }),
              className
            )}
          >
            {shouldFallbackMemo ? (
              <GHIcon className="size-3" />
            ) : (
              <>
                <GHIcon className="size-3" />
                <Separator orientation="vertical" className="h-3" />
                <StarIcon className="size-3 text-yellow-500" />
                <span>
                  {formatNumberWithCommas(
                    repository?.stargazers_count as number
                  )}
                </span>
              </>
            )}
          </BadgeLink>
        </TooltipTrigger>
        <TooltipContent className="max-w-md">
          {helpTextIsLoading ? (
            <SpinnerLoading className="size-4" />
          ) : (
            <p>{helpText}</p>
          )}
        </TooltipContent>
      </Tooltip>
    </TooltipProvider>
  );
}

export function GHTryTENAgent(props: { className?: string }) {
  const { className } = props;

  const { t, i18n } = useTranslation();
  const {
    data: helpText,
    error: helpTextError,
    isLoading: helpTextIsLoading,
  } = useHelpText(EHelpTextKey.TEN_AGENT, i18n.language);

  React.useEffect(() => {
    if (helpTextError) {
      console.error(helpTextError);
    }
  }, [helpTextError]);

  return (
    <TooltipProvider delayDuration={100}>
      <Tooltip>
        <TooltipTrigger asChild>
          <BadgeLink
            onClick={(e) => {
              e.preventDefault();
              window.open(TEN_AGENT_GITHUB_URL, "_blank");
              window.open(TEN_AGENT_URL, "_blank");
            }}
            target="_blank"
            rel="noopener noreferrer"
            className={cn(
              "flex items-center gap-1.5",
              "text-sm",
              badgeVariants({ variant: "secondary" }),
              className
            )}
          >
            <BotIcon className="size-3" />
            <Separator orientation="vertical" className="h-3" />
            <span className="">{t("header.tryTENAgent")}</span>
          </BadgeLink>
        </TooltipTrigger>
        <TooltipContent className="max-w-md">
          {helpTextIsLoading ? (
            <SpinnerLoading className="size-4" />
          ) : (
            <p>{helpText}</p>
          )}
        </TooltipContent>
      </Tooltip>
    </TooltipProvider>
  );
}

const BadgeLink = React.forwardRef<
  HTMLAnchorElement,
  React.AnchorHTMLAttributes<HTMLAnchorElement> &
    VariantProps<typeof badgeVariants>
>((props, ref) => {
  const { className, ...rest } = props;
  return <a className={cn(className)} {...rest} ref={ref} />;
});
