//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as React from "react";

import {
  makeAPIRequest,
  prepareReqUrl,
  getQueryHookCache,
} from "@/api/services/utils";
import { ENDPOINT_DOC_LINK } from "@/api/endpoints";
import { ENDPOINT_METHOD } from "@/api/endpoints/constant";
import { EDocLinkKey } from "@/types/doc";
import { localeStringToEnum, getShortLocale } from "@/api/services/utils";

export const retrieveDocLink = async (key: EDocLinkKey, locale?: string) => {
  const template = ENDPOINT_DOC_LINK.retrieveDocLink[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: { key, locale: localeStringToEnum(locale) },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

// TODO: refine this hook(post should not be used)
export const useDocLink = (key: EDocLinkKey, locale?: string) => {
  const template = ENDPOINT_DOC_LINK.retrieveDocLink[ENDPOINT_METHOD.POST];
  const url = prepareReqUrl(template) + `${key}/${localeStringToEnum(locale)}`;
  const queryHookCache = getQueryHookCache();

  const [data, setData] = React.useState<{
    key: string;
    locale: string;
    text: string;
  } | null>(() => {
    const [cachedData, cachedDataIsExpired] = queryHookCache.get<{
      key: string;
      locale: string;
      text: string;
    }>(url);
    if (!cachedData || cachedDataIsExpired) {
      return null;
    }
    return cachedData;
  });
  const [error, setError] = React.useState<Error | null>(null);
  const [isLoading, setIsLoading] = React.useState<boolean>(false);

  const fetchData = React.useCallback(async () => {
    setIsLoading(true);
    try {
      const res = await retrieveDocLink(key, locale);
      setData(res);
      queryHookCache.set(url, res, {
        ttl: 1000 * 60 * 60 * 24, // 1 day
      });
    } catch (err) {
      setError(err as Error);
    } finally {
      setIsLoading(false);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [key, locale, url]);

  React.useEffect(() => {
    fetchData();
  }, [fetchData]);

  return {
    data: { ...data, shortLocale: getShortLocale(data?.locale) },
    error,
    isLoading,
    mutate: fetchData,
  };
};
