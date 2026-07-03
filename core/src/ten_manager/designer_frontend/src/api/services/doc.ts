//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { useMutation, useQuery } from "@tanstack/react-query";
import { ENDPOINT_DOC_LINK } from "@/api/endpoints";
import { ENDPOINT_METHOD } from "@/api/endpoints/constant";
import {
  getTanstackQueryClient,
  localeStringToEnum,
  makeAPIRequest,
} from "@/api/services/utils";
import type { EDocLinkKey } from "@/types/doc";

export const retrieveDocLink = async (key: EDocLinkKey, locale?: string) => {
  const template = ENDPOINT_DOC_LINK.retrieveDocLink[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: { key, locale: localeStringToEnum(locale) },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const useRetrieveDocLink = (key: EDocLinkKey, locale?: string) => {
  const queryClient = getTanstackQueryClient();
  const queryKey = ["retrieveDocLink", key, localeStringToEnum(locale)];
  const { isPending, data, error } = useQuery({
    queryKey,
    queryFn: () => retrieveDocLink(key, locale),
  });
  const mutation = useMutation({
    mutationFn: () => retrieveDocLink(key, locale),
    onSuccess: () => {
      // Invalidate and refetch
      queryClient.invalidateQueries({
        queryKey,
      });
    },
  });

  return {
    data: data,
    error,
    isLoading: isPending,
    mutate: mutation.mutate,
  };
};
