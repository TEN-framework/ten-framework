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
import { ENDPOINT_APPS, ENDPOINT_TEMPLATES } from "@/api/endpoints";
import { ENDPOINT_METHOD } from "@/api/endpoints/constant";
import { ETemplateLanguage, ETemplateType } from "@/types/apps";

export const getApps = async () => {
  const template = ENDPOINT_APPS.apps[ENDPOINT_METHOD.GET];
  const req = makeAPIRequest(template);
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const useApps = () => {
  const template = ENDPOINT_APPS.apps[ENDPOINT_METHOD.GET];
  const url = prepareReqUrl(template);
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

export const postLoadDir = async (baseDir: string) => {
  const template = ENDPOINT_APPS.loadApps[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: { base_dir: baseDir },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const postReloadApps = async (baseDir?: string) => {
  const template = ENDPOINT_APPS.reloadApps[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: baseDir ? { base_dir: baseDir } : {},
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const postUnloadApps = async (baseDir: string) => {
  const template = ENDPOINT_APPS.unloadApps[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: { base_dir: baseDir },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const retrieveAppScripts = async (baseDir: string) => {
  const template = ENDPOINT_APPS.appScripts[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: { base_dir: baseDir },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

// TODO: refine this hook(post should not be used)
export const useAppScripts = (baseDir: string) => {
  const template = ENDPOINT_APPS.appScripts[ENDPOINT_METHOD.POST];
  const url = prepareReqUrl(template) + `${encodeURIComponent(baseDir)}`;
  const queryHookCache = getQueryHookCache();

  const [scripts, setScripts] = React.useState<string[]>(() => {
    const [cachedData, cachedDataIsExpired] = queryHookCache.get<{
      scripts: string[];
    }>(url);
    if (!cachedData || cachedDataIsExpired) {
      return [];
    }
    return cachedData.scripts;
  });
  const [isLoading, setIsLoading] = React.useState<boolean>(false);
  const [error, setError] = React.useState<Error | null>(null);

  const fetchScripts = React.useCallback(async () => {
    setIsLoading(true);
    try {
      const data = await retrieveAppScripts(baseDir);
      setScripts(data?.scripts || []);
      queryHookCache.set(url, data);
    } catch (error) {
      setError(error as Error);
    } finally {
      setIsLoading(false);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [baseDir, url]);

  const mutate = React.useCallback(() => {
    queryHookCache.delete(url);
    fetchScripts();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [fetchScripts, url]);

  React.useEffect(() => {
    fetchScripts();
  }, [fetchScripts]);

  return { data: scripts, isLoading, error, mutate };
};

// TODO: refine this hook(post should not be used)
export const retrieveTemplatePkgs = async (
  pkgType: ETemplateType,
  language: ETemplateLanguage
) => {
  const template = ENDPOINT_TEMPLATES.templatePkgs[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: { pkg_type: pkgType, language: language },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const postCreateApp = async (
  baseDir: string,
  templateName: string,
  appName: string
) => {
  const template = ENDPOINT_APPS.createApp[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: { base_dir: baseDir, template_name: templateName, app_name: appName },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};
