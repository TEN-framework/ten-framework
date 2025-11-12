//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import { useMutation, useQuery } from "@tanstack/react-query";
import type { z } from "zod";
import { ENDPOINT_MESSAGES } from "@/api/endpoints";
import { ENDPOINT_METHOD } from "@/api/endpoints/constant";
import { getTanstackQueryClient, makeAPIRequest } from "@/api/services/utils";
import type { MsgCompatiblePayloadSchema } from "@/types/graphs";

export const retrieveCompatibleMessages = async (
  payload?: z.infer<typeof MsgCompatiblePayloadSchema> | null
) => {
  if (!payload) {
    return [];
  }
  const template = ENDPOINT_MESSAGES.compatible[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: payload,
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const useCompatibleMessages = (
  payload: z.infer<typeof MsgCompatiblePayloadSchema> | null = null
) => {
  const queryClient = getTanstackQueryClient();
  const queryKey = ["compatible", [ENDPOINT_METHOD.POST], payload];
  const { isPending, data, error } = useQuery({
    queryKey,
    queryFn: () => retrieveCompatibleMessages(payload),
    enabled: !!payload, // Only run the query if payload is not null
  });
  const mutation = useMutation({
    mutationFn: () => retrieveCompatibleMessages(payload),
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
