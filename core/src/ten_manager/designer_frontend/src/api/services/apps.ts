//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import { useMutation, useQuery } from "@tanstack/react-query";
import type z from "zod";
import { ENDPOINT_APPS, ENDPOINT_TEMPLATES } from "@/api/endpoints";
import { ENDPOINT_METHOD } from "@/api/endpoints/constant";
import { getTanstackQueryClient, makeAPIRequest } from "@/api/services/utils";
import type {
  AppCreateReqSchema,
  ETemplateLanguage,
  ETemplateType,
} from "@/types/apps";

export const getApps = async () => {
  const template = ENDPOINT_APPS.apps[ENDPOINT_METHOD.GET];
  const req = makeAPIRequest(template);
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const useFetchApps = () => {
  const queryClient = getTanstackQueryClient();
  const queryKey = ["apps", ENDPOINT_METHOD.GET];
  const { isPending, data, error } = useQuery({
    queryKey,
    queryFn: getApps,
  });
  const mutation = useMutation({
    mutationFn: getApps,
    onSuccess: () => {
      // Invalidate and refetch
      queryClient.invalidateQueries({
        queryKey,
      });
    },
  });

  return {
    data,
    error,
    isLoading: isPending,
    mutate: mutation.mutate,
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

export const useFetchAppScripts = (baseDir: string) => {
  const queryClient = getTanstackQueryClient();
  const queryKey = ["appScripts", baseDir];
  const { isPending, data, error } = useQuery({
    queryKey,
    queryFn: () => retrieveAppScripts(baseDir),
  });
  const mutation = useMutation({
    mutationFn: () => retrieveAppScripts(baseDir),
    onSuccess: () => {
      // Invalidate and refetch
      queryClient.invalidateQueries({
        queryKey,
      });
    },
  });

  return {
    data: data?.scripts || [],
    error,
    isLoading: isPending,
    mutate: mutation.mutate,
  };
};

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
  payload: z.infer<typeof AppCreateReqSchema>
) => {
  const template = ENDPOINT_APPS.createApp[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: payload,
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};
