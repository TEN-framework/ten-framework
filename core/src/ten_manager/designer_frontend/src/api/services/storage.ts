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
import { ENDPOINT_PREFERENCES, ENDPOINT_STORAGE } from "@/api/endpoints";
import { ENDPOINT_METHOD } from "@/api/endpoints/constant";
import { PERSISTENT_SCHEMA, PERSISTENT_DEFAULTS } from "@/constants/persistent";

export const usePreferencesLogViewerLines = () => {
  const template =
    ENDPOINT_PREFERENCES.logviewer_line_size[ENDPOINT_METHOD.GET];
  const url = prepareReqUrl(template);
  const [{ data, error, isLoading }] = useCancelableSWR<
    z.infer<typeof template.responseSchema>
  >(url, {
    revalidateOnFocus: false,
    refreshInterval: 0,
  });
  return {
    data: data?.data,
    error,
    isLoading,
  };
};

export const updatePreferencesLogViewerLines = async (size: number) => {
  const template =
    ENDPOINT_PREFERENCES.logviewer_line_size[ENDPOINT_METHOD.PUT];
  const req = makeAPIRequest(template, {
    body: { logviewer_line_size: size },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const getStorageValueByKey = async (
  queryKey?: string,
  options?: {
    storageType?: "in-memory" | "persistent";
  }
) => {
  const key = queryKey || "properties"; // Default key if not provided
  const template =
    options?.storageType === "persistent"
      ? ENDPOINT_STORAGE.persistentGet[ENDPOINT_METHOD.POST]
      : ENDPOINT_STORAGE.inMemoryGet[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: { key },
  });
  const res = await req;
  return template.responseSchema.parse(res).data.value;
};

export const setStorageValueByKey = async (
  setKey?: string,
  setVal?: unknown,
  options?: {
    storageType?: "in-memory" | "persistent";
  }
) => {
  const key = setKey || "properties"; // Default key if not provided
  const value = setVal || PERSISTENT_DEFAULTS; // Default value if not provided
  const template =
    options?.storageType === "persistent"
      ? ENDPOINT_STORAGE.persistentSet[ENDPOINT_METHOD.POST]
      : ENDPOINT_STORAGE.inMemorySet[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: { key, value },
  });
  const res = await req;
  return template.responseSchema.parse(res).data.success;
};

export const setPersistentStorageSchema = async (
  schema: z.infer<z.ZodTypeAny>
) => {
  const template = ENDPOINT_STORAGE.persistentSchema[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: { schema },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const initPersistentStorageSchema = async () => {
  const res = await setPersistentStorageSchema(PERSISTENT_SCHEMA);
  return res;
};

export const useStorage = <T>(type?: "in-memory" | "persistent") => {
  const template =
    type === "persistent"
      ? ENDPOINT_STORAGE.persistentGet[ENDPOINT_METHOD.POST]
      : ENDPOINT_STORAGE.inMemoryGet[ENDPOINT_METHOD.POST];
  const url = prepareReqUrl(template) + type;
  const queryHookCache = getQueryHookCache();

  const [data, setData] = React.useState<T | null>(() => {
    const [cachedData, cachedDataIsExpired] = queryHookCache.get<T>(url);
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
      const req = makeAPIRequest(template, {
        body: {
          key: "properties",
        },
      });
      const res = await req;
      const parsedData = template.responseSchema.parse(res).data as T;
      setData(parsedData);
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
