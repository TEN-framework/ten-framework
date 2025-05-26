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
import { ENDPOINT_PREFERENCES } from "@/api/endpoints";
import { ENDPOINT_METHOD } from "@/api/endpoints/constant";

export const usePreferencesLogViewerLines = () => {
  const template =
    ENDPOINT_PREFERENCES.logviewer_line_size[ENDPOINT_METHOD.GET];
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

export const updatePreferencesLogViewerLines = async (size: number) => {
  const template =
    ENDPOINT_PREFERENCES.logviewer_line_size[ENDPOINT_METHOD.PUT];
  const req = makeAPIRequest(template, {
    body: { logviewer_line_size: size },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};
