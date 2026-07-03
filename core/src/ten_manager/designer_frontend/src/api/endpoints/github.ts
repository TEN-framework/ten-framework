//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import z from "zod";

import { API_GH_ROOT, ENDPOINT_METHOD } from "@/api/endpoints/constant";

export const ENDPOINT_GH = {
  // https://docs.github.com/en/rest/repos/repos?apiVersion=2022-11-28#get-a-repository
  repository: {
    [ENDPOINT_METHOD.GET]: {
      url: `${API_GH_ROOT}/repos/:owner/:repo`,
      method: ENDPOINT_METHOD.GET,
      pathParams: ["owner", "repo"],
      // Partial schema, add more fields as needed.
      responseSchema: z.object({
        id: z.number(),
        name: z.string(),
        full_name: z.string(),
        private: z.boolean(),
        html_url: z.string(),
        description: z.string(),
        stargazers_count: z.number(),
        watchers_count: z.number(),
        forks_count: z.number(),
        open_issues_count: z.number(),
      }),
    },
  },
};
