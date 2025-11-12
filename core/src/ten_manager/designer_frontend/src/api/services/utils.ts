//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import { QueryClient } from "@tanstack/react-query";
import useSWR, { type SWRConfiguration, type SWRResponse } from "swr";
import type { z } from "zod";
import type { IReqTemplate } from "@/api/endpoints";
import type { ENDPOINT_METHOD } from "@/api/endpoints/constant";
import logger from "@/logger";
import { EPreferencesLocale } from "@/types/apps";
import type { TenCloudStorePackageSchemaI18nField } from "@/types/extension";

export const prepareReqUrl = (
  reqTemplate: IReqTemplate<ENDPOINT_METHOD, unknown>,
  opts?: {
    query?: Record<string, string | undefined>;
    pathParams?: Record<string, string>;
  }
): string => {
  // 1. prepare url
  let url = reqTemplate.url;
  logger.debug({ scope: "api", module: "utils", data: { url } }, "prepare url");
  // 2. append query params
  if (opts?.query) {
    const searchParams = new URLSearchParams();
    Object.entries(opts.query).forEach(([key, value]) => {
      if (value !== undefined) {
        searchParams.append(key, value);
      }
    });
    const searchParamsStr = searchParams.toString();
    url = searchParamsStr ? `${url}?${searchParamsStr}` : url;
    logger.debug(
      { scope: "api", module: "utils", data: { url } },
      "append query params"
    );
  }
  // 3. validate path params
  if (reqTemplate.pathParams) {
    const missingParams = reqTemplate.pathParams.filter(
      (param) =>
        opts?.pathParams === undefined || opts.pathParams[param] === undefined
    );
    logger.debug(
      { scope: "api", module: "utils", data: { missingParams } },
      "validate path params"
    );
    if (missingParams.length > 0) {
      logger.error(
        { scope: "api", module: "utils", data: { missingParams } },
        "missing required path parameters"
      );
      throw new Error(
        `Missing required path parameters: ${missingParams.join(", ")}`
      );
    }
  }
  // 4. replace path params
  if (opts?.pathParams) {
    Object.entries(opts.pathParams).forEach(([key, value]) => {
      url = url.replace(`:${key}`, value);
    });
    logger.debug(
      { scope: "api", module: "utils", data: { url } },
      "replace path params"
    );
  }
  return url;
};

/**
 * Parse request template and return a fetch request
 * @param reqTemplate - Request template
 * @param opts - Options
 * @param fetchOpts - Fetch options
 * @returns Fetch request
 */
export const parseReq = <T extends ENDPOINT_METHOD>(
  reqTemplate: IReqTemplate<T, unknown>,
  opts?: {
    query?: Record<string, string | undefined>;
    pathParams?: Record<string, string>;
    body?: Record<string, unknown>;
  },
  fetchOpts?: RequestInit
) => {
  const url = prepareReqUrl(reqTemplate, opts);
  // 5. prepare fetch options
  const { headers: inputHeaders, ...restInput } = fetchOpts ?? {};
  const headers = {
    "Content-Type": "application/json",
    ...inputHeaders,
  };
  // 6. return fetch
  logger.debug(
    {
      scope: "api",
      module: "utils",
      data: {
        url,
        headers,
        method: reqTemplate.method,
        body: opts?.body,
      },
    },
    "prepare fetch request"
  );
  return fetch(url, {
    headers,
    ...restInput,
    method: reqTemplate.method,
    ...(opts?.body ? { body: JSON.stringify(opts.body) } : {}),
  });
};

export class APIError extends Error {}

export const parseResponseError = async (res: Response) => {
  try {
    const errorData = await res.json();
    throw new APIError(errorData.message || "Unknown error occurred");
  } catch (error) {
    if (error instanceof APIError) {
      throw error;
    }
    throw new Error(error as string);
  }
};

export const makeAPIRequest = async <T extends ENDPOINT_METHOD, R = unknown>(
  reqTemplate: IReqTemplate<T, R>,
  opts?: {
    query?: Record<string, string | undefined>;
    pathParams?: Record<string, string>;
    body?: Record<string, unknown>;
  },
  fetchOpts?: RequestInit
): Promise<R> => {
  const req = parseReq(reqTemplate, opts, fetchOpts);
  const res = await req;
  if (!res.ok) {
    await parseResponseError(res);
  }
  const data = await res.json();
  logger.debug(
    { scope: "api", module: "request", data: { data } },
    "request success"
  );
  return data;
};

/**
 * @deprecated
 * This hook is deprecated and will be removed in the future.
 *
 * Currently, swr is not compatible with POST requests
 *
 * Use tanstack query instead
 */
// https://github.com/vercel/swr/discussions/2330#discussioncomment-4460054
export function useCancelableSWR<T>(
  key: string,
  opts?: SWRConfiguration
): [SWRResponse<T>, AbortController] {
  logger.debug(
    { scope: "api", module: "swr", data: { key, opts } },
    "preparing SWR request"
  );
  const controller = new AbortController();
  return [
    useSWR(
      key,
      (url: string) =>
        fetch(url, { signal: controller.signal }).then((res) => res.json()),
      {
        // revalidateOnFocus: false,
        errorRetryCount: 3,
        refreshInterval: 1000 * 60,
        // dedupingInterval: 30000,
        // focusThrottleInterval: 60000,
        ...opts,
      }
    ),
    controller,
  ];
  // to use it:
  // const [{ data }, controller] = useCancelableSWR('/api')
  // ...
  // controller.abort()
}

export const localeStringToEnum = (locale?: string) => {
  switch (locale) {
    case "zh-CN":
      return EPreferencesLocale.ZH_CN;
    case "zh-TW":
      return EPreferencesLocale.ZH_TW;
    case "ja-JP":
      return EPreferencesLocale.JA_JP;
    // case "en-US":
    default:
      return EPreferencesLocale.EN_US;
  }
};

export const getShortLocale = (locale?: string) => {
  const inputLocale = locale ?? EPreferencesLocale.EN_US;

  return inputLocale.split("-")?.[0]?.toLowerCase();
};

export const getFullLocale = (locale?: string) => {
  const inputLocale = locale ?? EPreferencesLocale.EN_US;
  const target = localeStringToEnum(inputLocale);
  return target;
};

export const extractLocaleContentFromPkg = (
  data?: z.infer<typeof TenCloudStorePackageSchemaI18nField>,
  locale?: string
): string | undefined => {
  if (!data) {
    return undefined;
  }
  const targetLocale = getFullLocale(locale);
  const targetContent = data.locales[targetLocale]?.content;
  if (targetContent) {
    return targetContent;
  }
  const defaultLocale = EPreferencesLocale.EN_US;
  const defaultContent = data.locales[defaultLocale]?.content;
  return defaultContent;
};

let _tanstackQueryClient: QueryClient | null = null;

export const getTanstackQueryClient = () => {
  if (!_tanstackQueryClient) {
    _tanstackQueryClient = new QueryClient();
  }
  return _tanstackQueryClient;
};
