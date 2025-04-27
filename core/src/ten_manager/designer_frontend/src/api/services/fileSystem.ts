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
import { ENDPOINT_FILE_SYSTEM, ENDPOINT_FILE_VALIDATE } from "@/api/endpoints";
import { ENDPOINT_METHOD } from "@/api/endpoints/constant";

import type { IBaseDirResponse } from "@/types/fileSystem";

// request functions -------------------------------

export const retrieveFileContent = async (path: string) => {
  const template = ENDPOINT_FILE_SYSTEM.fileContent[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: {
      file_path: path,
    },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const putFileContent = async (
  path: string,
  data: { content: string }
) => {
  const template = ENDPOINT_FILE_SYSTEM.fileContent[ENDPOINT_METHOD.PUT];
  const req = makeAPIRequest(template, {
    body: {
      file_path: path,
      content: data.content,
    },
  });
  const res = await req;
  return res;
};

/** @deprecated */
export const getDirList = async (path: string) => {
  const encodedPath = encodeURIComponent(path);
  const template = ENDPOINT_FILE_SYSTEM.dirList[ENDPOINT_METHOD.GET];
  const req = makeAPIRequest(template, {
    pathParams: { path: encodedPath },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const retrieveDirList = async (path: string) => {
  const template = ENDPOINT_FILE_SYSTEM.dirList[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: { path },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

// query hooks -------------------------------

// TODO: refine this hook(post should not be used)
export const useFileContent = (path: string) => {
  const template = ENDPOINT_FILE_SYSTEM.fileContent[ENDPOINT_METHOD.POST];
  const url = prepareReqUrl(template) + `${encodeURIComponent(path)}`;
  const queryHookCache = getQueryHookCache();

  const [data, setData] = React.useState<{ content: string } | null>(() => {
    const [cachedData, cachedDataIsExpired] = queryHookCache.get<{
      content: string;
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
      const res = await retrieveFileContent(path);
      setData(res);
      queryHookCache.set(url, res);
    } catch (err) {
      setError(err as Error);
    } finally {
      setIsLoading(false);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [path, url]);

  React.useEffect(() => {
    fetchData();
  }, [fetchData]);

  return {
    content: data?.content,
    error,
    isLoading,
    mutate: fetchData,
  };
};

/** @deprecated */
export const useDirList = (path: string) => {
  const template = ENDPOINT_FILE_SYSTEM.dirList[ENDPOINT_METHOD.GET];
  const url = prepareReqUrl(template, {
    pathParams: { path: encodeURIComponent(path) },
  });
  const [{ data, error, isLoading, mutate }, controller] = useCancelableSWR<
    z.infer<typeof template.responseSchema>
  >(url, {
    revalidateOnFocus: false,
    refreshInterval: 0,
    errorRetryCount: 0,
  });
  return {
    data: data?.data,
    error,
    isLoading,
    mutate,
    controller,
  };
};

// TODO: refine this hook(post should not be used)
export const useRetrieveDirList = (path: string) => {
  const template = ENDPOINT_FILE_SYSTEM.dirList[ENDPOINT_METHOD.POST];
  const url = prepareReqUrl(template) + `${encodeURIComponent(path)}`;
  const queryHookCache = getQueryHookCache();

  const [data, setData] = React.useState<IBaseDirResponse | null>(() => {
    const [cachedData, cachedDataIsExpired] =
      queryHookCache.get<IBaseDirResponse>(url);
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
      const res = await retrieveDirList(path);
      setData(res);
      queryHookCache.set(url, res);
    } catch (err) {
      setError(err as Error);
    } finally {
      setIsLoading(false);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [path, url]);

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

export const validateProperty = async (fileContent: string) => {
  const template = ENDPOINT_FILE_VALIDATE.property[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: {
      property_json_str: fileContent,
    },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const validateManifest = async (fileContent: string) => {
  const template = ENDPOINT_FILE_VALIDATE.manifest[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: {
      manifest_json_str: fileContent,
    },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};
