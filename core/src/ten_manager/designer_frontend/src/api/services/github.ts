//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { useMutation, useQuery } from "@tanstack/react-query";
import { ENDPOINT_GH } from "@/api/endpoints";
import { ENDPOINT_METHOD } from "@/api/endpoints/constant";
import { getTanstackQueryClient, makeAPIRequest } from "@/api/services/utils";

export const retrieveGHRepository = async (options: {
  owner: string;
  repo: string;
}) => {
  const template = ENDPOINT_GH.repository[ENDPOINT_METHOD.GET];
  const req = makeAPIRequest(template, {
    pathParams: { owner: options.owner, repo: options.repo },
  });
  const res = await req;
  return template.responseSchema.parse(res);
};

export const useGitHubRepository = (options: {
  owner: string;
  repo: string;
}) => {
  const queryClient = getTanstackQueryClient();
  const queryKey = ["ghRepository", ENDPOINT_METHOD.GET, options];
  const { isPending, data, error } = useQuery({
    queryKey,
    queryFn: () => retrieveGHRepository(options),
  });
  const mutation = useMutation({
    mutationFn: () => retrieveGHRepository(options),
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
