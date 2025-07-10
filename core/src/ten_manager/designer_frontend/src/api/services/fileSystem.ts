//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { useMutation, useQuery } from "@tanstack/react-query";
import { ENDPOINT_FILE_SYSTEM, ENDPOINT_FILE_VALIDATE } from "@/api/endpoints";
import { ENDPOINT_METHOD } from "@/api/endpoints/constant";
import { getTanstackQueryClient, makeAPIRequest } from "@/api/services/utils";

// request functions -------------------------------

export const retrieveFileContent = async (path: string) => {
  const template = ENDPOINT_FILE_SYSTEM.fileContent[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: {
      file_path: path,
    },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const putFileContent = async (
  path: string,
  data: { content: string }
) => {
  const template = ENDPOINT_FILE_SYSTEM.fileContent[ENDPOINT_METHOD.PUT];
  const req = makeAPIRequest(template, {
    body: {
      file_path: path,
      content: data.content,
    },
  });
  const res = await req;
  return res;
};

export const retrieveDirList = async (path: string) => {
  const template = ENDPOINT_FILE_SYSTEM.dirList[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: { path },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

// query hooks -------------------------------

export const useRetrieveFileContent = (
  path: string,
  defaultContent?: string
) => {
  const queryClient = getTanstackQueryClient();
  const queryKey = ["fileContent", ENDPOINT_METHOD.POST, path];
  const { isPending, data, error } = useQuery({
    queryKey,
    queryFn: () => retrieveFileContent(path),
    initialData: { content: defaultContent ?? "" },
  });
  const mutation = useMutation({
    mutationFn: () => retrieveFileContent(path),
    onSuccess: () => {
      // Invalidate and refetch
      queryClient.invalidateQueries({
        queryKey,
      });
    },
  });

  return {
    data: data?.content,
    error,
    isLoading: isPending,
    mutate: mutation.mutate,
  };
};

export const useRetrieveDirList = (path: string) => {
  const queryClient = getTanstackQueryClient();
  const queryKey = ["dirList", ENDPOINT_METHOD.POST, path];
  const { isPending, data, error } = useQuery({
    queryKey,
    queryFn: () => retrieveDirList(path),
  });
  const mutation = useMutation({
    mutationFn: () => retrieveDirList(path),
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

export const validateProperty = async (fileContent: string) => {
  const template = ENDPOINT_FILE_VALIDATE.property[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: {
      property_json_str: fileContent,
    },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const validateManifest = async (fileContent: string) => {
  const template = ENDPOINT_FILE_VALIDATE.manifest[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: {
      manifest_json_str: fileContent,
    },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};
