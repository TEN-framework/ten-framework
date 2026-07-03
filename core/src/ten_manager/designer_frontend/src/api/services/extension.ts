//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { useMutation, useQuery } from "@tanstack/react-query";
import type { z } from "zod";
import { ENDPOINT_METHOD } from "@/api/endpoints/constant";
import { ENDPOINT_EXTENSION } from "@/api/endpoints/extension";
import { getTanstackQueryClient, makeAPIRequest } from "@/api/services/utils";
import type {
  TenPackageQueryFilterSchema,
  TenPackageQueryOptionsSchema,
} from "@/types/extension";

/**
 * @deprecated
 * Use {@link searchTenCloudStorePackages} instead.
 */
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

/**
 * @deprecated
 * Use {@link useSearchTenCloudStorePackages} instead.
 */
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

export const searchTenCloudStorePackages = async (
  filter?: z.infer<typeof TenPackageQueryFilterSchema>,
  options?: z.infer<typeof TenPackageQueryOptionsSchema>,
  signal?: AbortSignal
) => {
  if (!filter) {
    return { packages: [] };
  }
  const template =
    ENDPOINT_EXTENSION.searchRegistryPackages[ENDPOINT_METHOD.POST];
  const payload = template.requestPayload.parse({
    filter,
    options,
  });
  const req = makeAPIRequest(
    template,
    {
      body: payload,
    },
    { signal: signal || AbortSignal.timeout(10000) }
  );
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const useSearchTenCloudStorePackages = (
  payload?: {
    filter: z.infer<typeof TenPackageQueryFilterSchema>;
    options?: z.infer<typeof TenPackageQueryOptionsSchema>;
  } | null
) => {
  const queryClient = getTanstackQueryClient();
  const filter = payload?.filter || {
    field: "name",
    operator: "regex",
    value: ".*default.*",
  };
  const options = payload?.options || {
    scope: "name,version,hash,display_name,tags,downloadUrl,type,description",
  };
  const queryKey = [
    "searchRegistryPackages",
    ENDPOINT_METHOD.POST,
    filter,
    options,
  ];
  const { isLoading, data, error } = useQuery({
    queryKey,
    queryFn: ({ signal }) =>
      searchTenCloudStorePackages(filter, options, signal),
    enabled: payload !== null,
  });
  const mutation = useMutation({
    mutationFn: () => searchTenCloudStorePackages(filter, options),
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
    isLoading: isLoading,
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
