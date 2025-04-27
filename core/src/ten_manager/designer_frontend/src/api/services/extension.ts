//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import z from "zod";

import {
  makeAPIRequest,
  useCancelableSWR,
  prepareReqUrl,
} from "@/api/services/utils";
import { ENDPOINT_EXTENSION } from "@/api/endpoints/extension";
import { ENDPOINT_METHOD } from "@/api/endpoints/constant";

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
