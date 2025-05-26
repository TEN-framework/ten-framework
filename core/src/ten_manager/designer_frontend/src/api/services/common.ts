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
import { ENDPOINT_COMMON } from "@/api/endpoints";
import { ENDPOINT_METHOD } from "@/api/endpoints/constant";

/**
 * @deprecated Use useVersion instead.
 */
export const getVersion = async () => {
  const template = ENDPOINT_COMMON.version[ENDPOINT_METHOD.GET];
  const req = makeAPIRequest(template);
  const res = await req;
  return template.responseSchema.parse(res).data.version;
};

export const useVersion = () => {
  const template = ENDPOINT_COMMON.version[ENDPOINT_METHOD.GET];
  const url = prepareReqUrl(template);
  const [{ data, error, isLoading }] = useCancelableSWR<
    z.infer<typeof template.responseSchema>
  >(url, {
    revalidateOnFocus: false,
    refreshInterval: 0,
  });
  return {
    version: data?.data?.version,
    error,
    isLoading,
  };
};

export const useCheckUpdate = () => {
  const template = ENDPOINT_COMMON.checkUpdate[ENDPOINT_METHOD.GET];
  const url = prepareReqUrl(template);
  const [{ data, error, isLoading }] = useCancelableSWR<
    z.infer<typeof template.responseSchema>
  >(url, {
    revalidateOnFocus: false,
    refreshInterval: 0,
  });
  return {
    data: data?.data,
    error,
    isLoading,
  };
};

export const getEnv = async () => {
  const template = ENDPOINT_COMMON.env[ENDPOINT_METHOD.GET];
  const req = makeAPIRequest(template);
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const useEnv = () => {
  const template = ENDPOINT_COMMON.env[ENDPOINT_METHOD.GET];
  const url = prepareReqUrl(template);
  const [{ data, error, isLoading }] = useCancelableSWR<
    z.infer<typeof template.responseSchema>
  >(url, {
    revalidateOnFocus: false,
    refreshInterval: 0,
  });
  return {
    data: data?.data,
    error,
    isLoading,
  };
};
