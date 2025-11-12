//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import { useMutation, useQuery } from "@tanstack/react-query";
import type z from "zod";
import { ENDPOINT_PREFERENCES, ENDPOINT_STORAGE } from "@/api/endpoints";
import { ENDPOINT_METHOD } from "@/api/endpoints/constant";
import { getTanstackQueryClient, makeAPIRequest } from "@/api/services/utils";
import { PERSISTENT_DEFAULTS, PERSISTENT_SCHEMA } from "@/constants/persistent";
import type { IRunAppParams } from "@/types/apps";
import type {
  GraphUiNodeGeometrySchema,
  SetGraphUiPayloadSchema,
} from "@/types/graphs";

export const getPreferencesLogViewerLines = async () => {
  const template =
    ENDPOINT_PREFERENCES.logviewer_line_size[ENDPOINT_METHOD.GET];
  const req = makeAPIRequest(template);
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const usePreferencesLogViewerLines = () => {
  const queryClient = getTanstackQueryClient();
  const queryKey = ["logviewer_line_size", ENDPOINT_METHOD.GET];
  const { isPending, data, error } = useQuery({
    queryKey,
    queryFn: getPreferencesLogViewerLines,
  });
  const mutation = useMutation({
    mutationFn: getPreferencesLogViewerLines,
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

export const updatePreferencesLogViewerLines = async (size: number) => {
  const template =
    ENDPOINT_PREFERENCES.logviewer_line_size[ENDPOINT_METHOD.PUT];
  const req = makeAPIRequest(template, {
    body: { logviewer_line_size: size },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const getStorageValueByKey = async <T>(
  queryKey?: string,
  options?: {
    storageType?: "in-memory" | "persistent";
  }
) => {
  const key = queryKey || "properties"; // Default key if not provided
  const template =
    options?.storageType === "persistent"
      ? ENDPOINT_STORAGE.persistentGet[ENDPOINT_METHOD.POST]
      : ENDPOINT_STORAGE.inMemoryGet[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: { key },
  });
  const res = await req;
  return template.responseSchema.parse(res).data.value as T;
};

export const setStorageValueByKey = async (
  setKey?: string,
  setVal?: unknown,
  options?: {
    storageType?: "in-memory" | "persistent";
  }
) => {
  const key = setKey || "properties"; // Default key if not provided
  const value = setVal || PERSISTENT_DEFAULTS; // Default value if not provided
  const template =
    options?.storageType === "persistent"
      ? ENDPOINT_STORAGE.persistentSet[ENDPOINT_METHOD.POST]
      : ENDPOINT_STORAGE.inMemorySet[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: { key, value },
  });
  const res = await req;
  return template.responseSchema.parse(res).data.success;
};

export const setPersistentStorageSchema = async (
  schema: z.infer<z.ZodTypeAny>
) => {
  const template = ENDPOINT_STORAGE.persistentSchema[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: { schema },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const initPersistentStorageSchema = async () => {
  const res = await setPersistentStorageSchema(PERSISTENT_SCHEMA);
  return res;
};

export const useStorage = (type?: "in-memory" | "persistent") => {
  const queryClient = getTanstackQueryClient();
  const queryKey = ["ENDPOINT_STORAGE", ENDPOINT_METHOD.POST, type];
  const { isPending, data, error } = useQuery({
    queryKey,
    queryFn: () => getStorageValueByKey("properties", { storageType: type }),
  });
  const mutation = useMutation({
    mutationFn: () => getStorageValueByKey("properties", { storageType: type }),
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

export const addRecentRunApp = async (app: IRunAppParams) => {
  const {
    base_dir,
    script_name,
    stdout_is_log,
    stderr_is_log,
    run_with_agent,
  } = app;
  const data = (await getStorageValueByKey()) as unknown;
  await setStorageValueByKey(undefined, {
    ...(data || {}),
    recent_run_apps: [
      {
        base_dir: base_dir,
        script_name: script_name,
        stdout_is_log: stdout_is_log,
        stderr_is_log: stderr_is_log,
        run_with_agent: run_with_agent,
      },
      ...((data as { recent_run_apps: IRunAppParams[] })?.recent_run_apps ||
        []),
    ].slice(0, 3), // keep only the first 3
  });
};

export const postSetGraphNodeGeometry = async (
  data: z.infer<typeof SetGraphUiPayloadSchema>
) => {
  const originalData =
    await getStorageValueByKey<Record<string, unknown>>("graph_ui");
  const updatedData = {
    ...originalData,
    [data.graph_id]: data,
  };
  await setStorageValueByKey("graph_ui", updatedData);
};

export const postGetGraphNodeGeometry = async (
  graphId: string
): Promise<z.infer<typeof GraphUiNodeGeometrySchema>[]> => {
  const data = await getStorageValueByKey<Record<string, unknown>>("graph_ui");
  return (
    (
      data?.[graphId] as {
        graph_id: string;
        graph_geometry?: {
          nodes_geometry?: z.infer<typeof GraphUiNodeGeometrySchema>[];
        };
      }
    )?.graph_geometry?.nodes_geometry || []
  );
};
