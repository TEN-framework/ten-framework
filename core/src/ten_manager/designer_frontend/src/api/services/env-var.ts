//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import { useMutation, useQuery } from "@tanstack/react-query";
import {
  ENDPOINT_METHOD,
  ENV_VAR_AGORA_APP_CERT,
  ENV_VAR_AGORA_APP_ID,
} from "@/api/endpoints/constant";
import { getTanstackQueryClient, makeAPIRequest } from "@/api/services/utils";
import { ENDPOINT_ENV_VAR } from "../endpoints/env-var";

export const getEnvVar = async (name: string) => {
  const template = ENDPOINT_ENV_VAR.getEnvVar[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: { name },
  });
  const res = await req;
  return template.responseSchema.parse(res);
};

export const getRTCEnvVar = async () => {
  const template = ENDPOINT_ENV_VAR.getEnvVar[ENDPOINT_METHOD.POST];
  const reqAppId = makeAPIRequest(template, {
    body: template.requestSchema.parse({
      name: ENV_VAR_AGORA_APP_ID,
    }),
  });
  const reqAppCert = makeAPIRequest(template, {
    body: template.requestSchema.parse({
      name: ENV_VAR_AGORA_APP_CERT,
    }),
  });
  const [resAppId, resAppCert] = await Promise.all([reqAppId, reqAppCert]);
  const parsedAppId = template.responseSchema.parse(resAppId);
  const parsedAppCert = template.responseSchema.parse(resAppCert);
  return {
    appId: parsedAppId.value,
    appCert: parsedAppCert.value,
  };
};

export const useRTCEnvVar = () => {
  const cacheKey = `env-var-rtc`;

  const queryClient = getTanstackQueryClient();
  const queryKey = [cacheKey, ENDPOINT_METHOD.POST, {}];
  const { isPending, data, error } = useQuery({
    queryKey,
    queryFn: () => getRTCEnvVar(),
  });
  const mutation = useMutation({
    mutationFn: () => getRTCEnvVar(),
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
