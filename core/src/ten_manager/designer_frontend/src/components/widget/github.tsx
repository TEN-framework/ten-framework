//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import type { VariantProps } from "class-variance-authority";
import { BotIcon, StarIcon } from "lucide-react";
import * as React from "react";
import { useTranslation } from "react-i18next";
import { EHelpTextKey } from "@/api/endpoints";
import { useGitHubRepository } from "@/api/services/github";
import { useHelpText } from "@/api/services/help-text";
import { GHIcon } from "@/components/icons";
import { SpinnerLoading } from "@/components/status/loading";
import { badgeVariants } from "@/components/ui/badge";
import { Separator } from "@/components/ui/separator";
import {
  Tooltip,
  TooltipContent,
  TooltipProvider,
  TooltipTrigger,
} from "@/components/ui/tooltip";
import { TEN_AGENT_URL } from "@/constants";
import { cn, formatNumberWithCommas } from "@/lib/utils";

export function GHStargazersCount(props: {
  owner: string;
  repo: string;
  className?: string;
}) {
  const { owner, repo, className } = props;

  const { i18n } = useTranslation();
  const {
    data: repository,
    error,
    isLoading,
  } = useGitHubRepository({ owner, repo });
  const {
    data: helpText,
    error: helpTextError,
    isLoading: helpTextIsLoading,
  } = useHelpText({ key: EHelpTextKey.TEN_FRAMEWORK, locale: i18n.language });

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
            <p>{helpText?.text}</p>
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
  } = useHelpText({ key: EHelpTextKey.TEN_AGENT, locale: i18n.language });

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
            href={TEN_AGENT_URL}
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
            <p>{helpText?.text}</p>
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
