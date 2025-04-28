//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { Input } from "@/components/ui/Input";
import { AutoFormFieldProps } from "@autoform/react";
import React from "react";

export const NumberField: React.FC<AutoFormFieldProps> = ({
  inputProps,
  error,
  id,
}) => {
  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  const { key, ...props } = inputProps;

  return (
    <Input
      id={id}
      type="number"
      className={error ? "border-destructive" : ""}
      {...props}
    />
  );
};
