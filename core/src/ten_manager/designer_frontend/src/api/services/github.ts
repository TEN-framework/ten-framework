//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import z from "zod";

import { useCancelableSWR, prepareReqUrl } from "@/api/services/utils";
import { ENDPOINT_GH } from "@/api/endpoints";
import { ENDPOINT_METHOD } from "@/api/endpoints/constant";

export const useGHRepository = (owner: string, repo: string) => {
  const template = ENDPOINT_GH.repository[ENDPOINT_METHOD.GET];
  const url = prepareReqUrl(template, {
    pathParams: { owner, repo },
  });
  const [{ data, error, isLoading }] = useCancelableSWR<
    z.infer<typeof template.responseSchema>
  >(url, {
    revalidateOnFocus: false,
    refreshInterval: 0,
  });
  return {
    repository: data,
    error,
    isLoading,
  };
};
