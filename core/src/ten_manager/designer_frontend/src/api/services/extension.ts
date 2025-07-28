//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { useMutation, useQuery } from "@tanstack/react-query";
import { ENDPOINT_METHOD } from "@/api/endpoints/constant";
import { ENDPOINT_EXTENSION } from "@/api/endpoints/extension";
import { getTanstackQueryClient, makeAPIRequest } from "@/api/services/utils";

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
  const queryClient = getTanstackQueryClient();
  const queryKey = ["registryPackages", ENDPOINT_METHOD.GET, options];
  const { isPending, data, error } = useQuery({
    queryKey,
    queryFn: () => listTenCloudStorePackages(options),
  });
  const mutation = useMutation({
    mutationFn: () => listTenCloudStorePackages(options),
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

export const useFetchExtSchema = (
  options: {
    appBaseDir: string;
    addonName: string;
  } | null
) => {
  const queryClient = getTanstackQueryClient();
  const queryKey = ["schema", ENDPOINT_METHOD.POST, options];
  const { isPending, data, error } = useQuery({
    queryKey,
    queryFn: () =>
      options ? retrieveExtensionSchema(options) : Promise.resolve(null),
    enabled: !!options,
  });
  const mutation = useMutation({
    mutationFn: () =>
      options ? retrieveExtensionSchema(options) : Promise.resolve(null),
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
