//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { useMutation, useQuery } from "@tanstack/react-query";
import { ENDPOINT_ADDONS } from "@/api/endpoints";
import { ENDPOINT_METHOD } from "@/api/endpoints/constant";
import { getTanstackQueryClient, makeAPIRequest } from "@/api/services/utils";

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

export const useFetchAddons = (payload: {
  base_dir?: string | null;
  addon_name?: string | null;
  addon_type?: string | null;
}) => {
  const queryClient = getTanstackQueryClient();
  const queryKey = ["addons", ENDPOINT_METHOD.POST, payload];
  const { isPending, data, error } = useQuery({
    queryKey,
    queryFn: () =>
      retrieveAddons(
        payload as {
          base_dir: string;
          addon_name?: string;
          addon_type?: string;
        }
      ),
    enabled: Boolean(payload.base_dir),
    initialData: [],
  });
  const mutation = useMutation({
    mutationFn: () =>
      retrieveAddons(
        payload as {
          base_dir: string;
          addon_name?: string;
          addon_type?: string;
        }
      ),
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
