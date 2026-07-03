//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import { useMutation, useQuery } from "@tanstack/react-query";
import type { z } from "zod";
import { ENDPOINT_GRAPH_UI, ENDPOINT_GRAPHS } from "@/api/endpoints";
import { ENDPOINT_METHOD } from "@/api/endpoints/constant";
import { getTanstackQueryClient, makeAPIRequest } from "@/api/services/utils";
import type {
  AddConnectionPayloadSchema,
  AddNodePayloadSchema,
  DeleteConnectionPayloadSchema,
  DeleteNodePayloadSchema,
  GraphUiNodeGeometrySchema,
  SetGraphUiPayloadSchema,
  UpdateNodePropertyPayloadSchema,
} from "@/types/graphs";

export const retrieveGraphs = async () => {
  const template = ENDPOINT_GRAPHS.graphs[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: {},
  });
  const res = await req;

  const resp = await template.responseSchema.parseAsync(res);

  return resp.data;
};

export const useGraphs = () => {
  const queryClient = getTanstackQueryClient();
  const queryKey = ["graphs", ENDPOINT_METHOD.POST];
  const { isPending, data, error } = useQuery({
    queryKey,
    queryFn: retrieveGraphs,
  });
  const mutation = useMutation({
    mutationFn: retrieveGraphs,
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

export const postAddNode = async (
  data: z.infer<typeof AddNodePayloadSchema>
) => {
  const template = ENDPOINT_GRAPHS.addNode[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: data,
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const postDeleteNode = async (
  data: z.infer<typeof DeleteNodePayloadSchema>
) => {
  const template = ENDPOINT_GRAPHS.deleteNode[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: data,
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const postAddConnection = async (
  data: z.infer<typeof AddConnectionPayloadSchema>
) => {
  const template = ENDPOINT_GRAPHS.addConnection[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: data,
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const postDeleteConnection = async (
  data: z.infer<typeof DeleteConnectionPayloadSchema>
) => {
  const template = ENDPOINT_GRAPHS.deleteConnection[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: data,
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const postUpdateNodeProperty = async (
  data: z.infer<typeof UpdateNodePropertyPayloadSchema>
) => {
  const template = ENDPOINT_GRAPHS.nodesPropertyUpdate[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: data,
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const postReplaceNode = async (
  data: z.infer<typeof AddNodePayloadSchema>
) => {
  const template = ENDPOINT_GRAPHS.replaceNode[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: data,
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

/**
 * @deprecated
 * This endpoint is deprecated and will be removed in the future.
 * Use `persistentSchema` endpoint instead.
 */
export const postSetGraphNodeGeometry = async (
  data: z.infer<typeof SetGraphUiPayloadSchema>
) => {
  const template = ENDPOINT_GRAPH_UI.set[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: data,
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

/**
 * @deprecated
 * This endpoint is deprecated and will be removed in the future.
 * Use `persistentSchema` endpoint instead.
 */
export const postGetGraphNodeGeometry = async (
  graphId: string
): Promise<z.infer<typeof GraphUiNodeGeometrySchema>[]> => {
  const template = ENDPOINT_GRAPH_UI.get[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: { graph_id: graphId },
  });
  const res = await req;
  const data = template.responseSchema.parse(res).data;
  return data?.graph_geometry?.nodes_geometry || [];
};

export const postGraphsAutoStart = async (payload: {
  graph_id: string;
  auto_start: boolean;
}) => {
  const template = ENDPOINT_GRAPHS.graphsAutoStart[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: payload,
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};
