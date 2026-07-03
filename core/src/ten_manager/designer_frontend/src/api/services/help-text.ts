//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { useMutation, useQuery } from "@tanstack/react-query";
import { type EHelpTextKey, ENDPOINT_HELP_TEXT } from "@/api/endpoints";
import { ENDPOINT_METHOD } from "@/api/endpoints/constant";
import {
  getTanstackQueryClient,
  localeStringToEnum,
  makeAPIRequest,
} from "@/api/services/utils";

export const retrieveHelpText = async (option: {
  key: string;
  locale?: string;
}) => {
  const template = ENDPOINT_HELP_TEXT.helpText[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: { key: option.key, locale: localeStringToEnum(option.locale) },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const useHelpText = (option: { key: EHelpTextKey; locale?: string }) => {
  const queryClient = getTanstackQueryClient();
  const queryKey = ["helpText", ENDPOINT_METHOD.POST, option];
  const { isPending, data, error } = useQuery({
    queryKey,
    queryFn: () => retrieveHelpText(option),
  });
  const mutation = useMutation({
    mutationFn: () => retrieveHelpText(option),
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
