//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as React from "react";
import { z } from "zod";

import {
  makeAPIRequest,
  prepareReqUrl,
  getQueryHookCache,
} from "@/api/services/utils";
import { ENDPOINT_GRAPH_UI, ENDPOINT_GRAPHS } from "@/api/endpoints";
import { ENDPOINT_METHOD } from "@/api/endpoints/constant";

import type {
  AddNodePayloadSchema,
  DeleteNodePayloadSchema,
  IGraph,
  AddConnectionPayloadSchema,
  DeleteConnectionPayloadSchema,
  UpdateNodePropertyPayloadSchema,
  SetGraphUiPayloadSchema,
  GraphUiNodeGeometrySchema,
} from "@/types/graphs";

export const retrieveGraphNodes = async (graphId: string) => {
  const template = ENDPOINT_GRAPHS.nodes[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: { graph_id: graphId },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

export const retrieveGraphConnections = async (graphId: string) => {
  const template = ENDPOINT_GRAPHS.connections[ENDPOINT_METHOD.POST];
  const req = makeAPIRequest(template, {
    body: { graph_id: graphId },
  });
  const res = await req;
  return template.responseSchema.parse(res).data;
};

// TODO: refine this hook(post should not be used)
export const useGraphs = () => {
  const template = ENDPOINT_GRAPHS.graphs[ENDPOINT_METHOD.POST];
  const url = prepareReqUrl(template);
  const queryHookCache = getQueryHookCache();

  const [data, setData] = React.useState<IGraph[] | null>(() => {
    const [cachedData, cachedDataIsExpired] = queryHookCache.get<IGraph[]>(url);
    if (!cachedData || cachedDataIsExpired) {
      return null;
    }
    return cachedData;
  });
  const [error, setError] = React.useState<Error | null>(null);
  const [isLoading, setIsLoading] = React.useState<boolean>(false);

  const fetchData = React.useCallback(async () => {
    setIsLoading(true);
    try {
      const template = ENDPOINT_GRAPHS.graphs[ENDPOINT_METHOD.POST];
      const req = makeAPIRequest(template, {
        body: {},
      });
      const res = await req;
      const parsedData = template.responseSchema.parse(res).data;
      setData(parsedData);
      queryHookCache.set(url, parsedData);
    } catch (err) {
      setError(err as Error);
    } finally {
      setIsLoading(false);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [url]);

  React.useEffect(() => {
    fetchData();
  }, [fetchData]);

  return {
    graphs: data,
    error,
    isLoading,
    mutate: fetchData,
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
