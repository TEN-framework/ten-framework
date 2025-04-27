//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as React from "react";

import {
  makeAPIRequest,
  getQueryHookCache,
  prepareReqUrl,
} from "@/api/services/utils";
import { ENDPOINT_ADDONS } from "@/api/endpoints";
import { ENDPOINT_METHOD } from "@/api/endpoints/constant";

import type { IExtensionAddon } from "@/types/apps";

export const retrieveAddons = async (payload: {
  // base_dir is the base directory of the app to retrieve addons for.
  base_dir: string;

  // addon_name is the name of the addon to retrieve.
  addon_name?: string;

  // addon_type is the type of the addon to retrieve.
  addon_type?: string;
}) => {
  const template = ENDPOINT_ADDONS.addons[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: payload,
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const useAddons = (baseDir?: string | null) => {
  const template = ENDPOINT_ADDONS.addons[ENDPOINT_METHOD.POST];
  const url =
    prepareReqUrl(template) + `${baseDir ? encodeURIComponent(baseDir) : ""}`;
  const queryHookCache = getQueryHookCache();

  const [data, setData] = React.useState<IExtensionAddon[] | null>(() => {
    const [cachedData, cachedDataIsExpired] =
      queryHookCache.get<IExtensionAddon[]>(url);
    if (!cachedData || cachedDataIsExpired) {
      return null;
    }
    return cachedData;
  });
  const [error, setError] = React.useState<Error | null>(null);
  const [isLoading, setIsLoading] = React.useState<boolean>(false);

  const fetchData = React.useCallback(async () => {
    if (baseDir === null) {
      return;
    }
    setIsLoading(true);
    try {
      const template = ENDPOINT_ADDONS.addons[ENDPOINT_METHOD.POST];
      const req = makeAPIRequest(template, {
        body: { base_dir: baseDir },
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
  }, [baseDir, url]);

  React.useEffect(() => {
    fetchData();
  }, [fetchData]);

  return {
    addons: data,
    error,
    isLoading,
    mutate: fetchData,
  };
};
