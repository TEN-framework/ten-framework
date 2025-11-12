//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

/** biome-ignore-all lint/correctness/noUnusedVariables: <ignore> */

import type { AutoFormFieldProps } from "@autoform/react";
import type React from "react";
import { Input } from "@/components/ui/input";

export const StringField: React.FC<AutoFormFieldProps> = ({
  inputProps,
  error,
  id,
}) => {
  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  const { key, ...props } = inputProps;

  return (
    <Input id={id} className={error ? "border-destructive" : ""} {...props} />
  );
};
