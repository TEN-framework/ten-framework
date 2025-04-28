//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
export const API_ENDPOINT_ROOT = "/api";

export enum API_ENDPOINT_CATEGORY {
  DESIGNER = "designer",
}

export enum API_ENDPOINT_VERSION {
  V1 = "v1",
}

export const API_DESIGNER_V1 =
  API_ENDPOINT_ROOT +
  "/" +
  API_ENDPOINT_CATEGORY.DESIGNER +
  "/" +
  API_ENDPOINT_VERSION.V1;

export enum ENDPOINT_METHOD {
  GET = "GET",
  POST = "POST",
  PUT = "PUT",
  DELETE = "DELETE",
  PATCH = "PATCH",
}

// Github
export const API_GH_ROOT = "https://api.github.com";
