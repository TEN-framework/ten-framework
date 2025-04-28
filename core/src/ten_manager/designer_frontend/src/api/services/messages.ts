//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as React from "react";
import { z } from "zod";

import {
  makeAPIRequest,
  prepareReqUrl,
  getQueryHookCache,
} from "@/api/services/utils";
import { ENDPOINT_MESSAGES } from "@/api/endpoints";
import { ENDPOINT_METHOD } from "@/api/endpoints/constant";
import {
  MsgCompatiblePayloadSchema,
  MsgCompatibleResponseItemSchema,
} from "@/types/graphs";

export const retrieveCompatibleMessages = async (
  payload: z.infer<typeof MsgCompatiblePayloadSchema>
) => {
  const template = ENDPOINT_MESSAGES.compatible[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: payload,
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

// TODO: refine this hook(post should not be used)
export const useCompatibleMessages = (
  payload: z.infer<typeof MsgCompatiblePayloadSchema> | null = null
) => {
  const template = ENDPOINT_MESSAGES.compatible[ENDPOINT_METHOD.POST];
  const url =
    prepareReqUrl(template) +
    `?${new URLSearchParams(payload as Record<string, string>)}`;
  const queryHookCache = getQueryHookCache();

  const [data, setData] = React.useState<
    z.infer<typeof MsgCompatibleResponseItemSchema>[] | null
  >(() => {
    const [cachedData, cachedDataIsExpired] =
      queryHookCache.get<z.infer<typeof MsgCompatibleResponseItemSchema>[]>(
        url
      );
    if (!cachedData || cachedDataIsExpired) {
      return null;
    }
    return cachedData;
  });
  const [error, setError] = React.useState<Error | null>(null);
  const [isLoading, setIsLoading] = React.useState<boolean>(false);

  const fetchData = React.useCallback(async () => {
    if (!payload) {
      return;
    }
    setIsLoading(true);
    try {
      const req = makeAPIRequest(template, {
        body: payload,
      });
      const res = await req;
      const parsedData = template.responseSchema.parse(res).data;
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
