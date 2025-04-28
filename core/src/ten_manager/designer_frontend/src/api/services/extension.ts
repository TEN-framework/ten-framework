//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as React from "react";
import z from "zod";

import {
  makeAPIRequest,
  useCancelableSWR,
  prepareReqUrl,
  getQueryHookCache,
} from "@/api/services/utils";
import { ENDPOINT_EXTENSION } from "@/api/endpoints/extension";
import { ENDPOINT_METHOD } from "@/api/endpoints/constant";
import { ExtensionSchema } from "@/types/extension";

export const listTenCloudStorePackages = async (options?: {
  page?: number;
  pageSize?: number;
}) => {
  const template = ENDPOINT_EXTENSION.registryPackages[ENDPOINT_METHOD.GET];
  const req = makeAPIRequest(template, {
    query: {
      page: options?.page?.toString() || undefined,
      pageSize: options?.pageSize?.toString() || undefined,
    },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const useListTenCloudStorePackages = (options?: {
  page?: number;
  pageSize?: number;
}) => {
  const template = ENDPOINT_EXTENSION.registryPackages[ENDPOINT_METHOD.GET];
  const url = prepareReqUrl(template, {
    query: {
      page: options?.page?.toString() || undefined,
      pageSize: options?.pageSize?.toString() || undefined,
    },
  });
  const [{ data, error, isLoading, mutate }] = useCancelableSWR<
    z.infer<typeof template.responseSchema>
  >(url, {
    revalidateOnFocus: false,
    refreshInterval: 0,
  });

  return {
    data: data?.data,
    error,
    isLoading,
    mutate,
  };
};

export const retrieveExtensionSchema = async (options: {
  appBaseDir: string;
  addonName: string;
}) => {
  const template = ENDPOINT_EXTENSION.schema[ENDPOINT_METHOD.POST];
  const payload = template.requestPayload.parse({
    app_base_dir: options.appBaseDir,
    addon_name: options.addonName,
  });
  const req = makeAPIRequest(template, {
    body: payload,
  });
  const res = await req;
  return template.responseSchema.parse(res).data.schema;
};

// TODO: refine this hook(post should not be used)
export const useExtensionSchema = (
  options: {
    appBaseDir: string;
    addonName: string;
  } | null
) => {
  const template = ENDPOINT_EXTENSION.schema[ENDPOINT_METHOD.POST];
  const url =
    prepareReqUrl(template) +
    `?app_base_dir=${options?.appBaseDir}&addon_name=${options?.addonName}`;
  const queryHookCache = getQueryHookCache();

  const [data, setData] = React.useState<z.infer<
    typeof ExtensionSchema
  > | null>(() => {
    const [cachedData, cachedDataIsExpired] =
      queryHookCache.get<z.infer<typeof ExtensionSchema>>(url);
    if (!cachedData || cachedDataIsExpired) {
      return null;
    }
    return cachedData;
  });
  const [error, setError] = React.useState<Error | null>(null);
  const [isLoading, setIsLoading] = React.useState<boolean>(false);

  const fetchData = React.useCallback(async () => {
    if (!options) {
      return;
    }
    setIsLoading(true);
    try {
      const req = makeAPIRequest(template, {
        body: template.requestPayload.parse({
          app_base_dir: options.appBaseDir,
          addon_name: options.addonName,
        }),
      });
      const res = await req;
      const parsedData = template.responseSchema.parse(res).data;
      setData(parsedData?.schema);
      queryHookCache.set(url, parsedData);
    } catch (err) {
      setError(err as Error);
    } finally {
      setIsLoading(false);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [url]);

  React.useEffect(() => {
    fetchData();
  }, [fetchData]);

  return {
    data,
    error,
    isLoading,
    mutate: fetchData,
  };
};

export const retrieveExtensionDefaultProperty = async (options: {
  appBaseDir: string;
  addonName: string;
}) => {
  const template = ENDPOINT_EXTENSION.getProperty[ENDPOINT_METHOD.POST];
  const payload = template.requestPayload.parse({
    app_base_dir: options.appBaseDir,
    addon_name: options.addonName,
  });
  const req = makeAPIRequest(template, {
    body: payload,
  });
  const res = await req;
  return template.responseSchema.parse(res).data.property;
};
