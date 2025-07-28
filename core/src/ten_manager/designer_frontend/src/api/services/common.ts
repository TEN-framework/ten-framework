//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { useMutation, useQuery } from "@tanstack/react-query";
import { ENDPOINT_COMMON } from "@/api/endpoints";
import { ENDPOINT_METHOD } from "@/api/endpoints/constant";
import { getTanstackQueryClient, makeAPIRequest } from "@/api/services/utils";

export const getVersion = async () => {
  const template = ENDPOINT_COMMON.version[ENDPOINT_METHOD.GET];
  const req = makeAPIRequest(template);
  const res = await req;
  return template.responseSchema.parse(res).data.version;
};

export const useFetchVersion = () => {
  const queryClient = getTanstackQueryClient();
  const queryKey = ["version", ENDPOINT_METHOD.GET];
  const { isPending, data, error } = useQuery({
    queryKey,
    queryFn: getVersion,
  });
  const mutation = useMutation({
    mutationFn: getVersion,
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

export const checkUpdate = async () => {
  const template = ENDPOINT_COMMON.checkUpdate[ENDPOINT_METHOD.GET];
  const req = makeAPIRequest(template);
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const useCheckUpdate = () => {
  const queryClient = getTanstackQueryClient();
  const queryKey = ["checkUpdate", ENDPOINT_METHOD.GET];
  const { isPending, data, error } = useQuery({
    queryKey,
    queryFn: checkUpdate,
  });
  const mutation = useMutation({
    mutationFn: checkUpdate,
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

export const getEnv = async () => {
  const template = ENDPOINT_COMMON.env[ENDPOINT_METHOD.GET];
  const req = makeAPIRequest(template);
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const useEnv = () => {
  const queryClient = getTanstackQueryClient();
  const queryKey = ["env", ENDPOINT_METHOD.GET];
  const { isPending, data, error } = useQuery({
    queryKey,
    queryFn: getEnv,
  });
  const mutation = useMutation({
    mutationFn: getEnv,
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
