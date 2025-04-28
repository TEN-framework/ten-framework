//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/Select";
import { AutoFormFieldProps } from "@autoform/react";
import React from "react";

export const SelectField: React.FC<AutoFormFieldProps> = ({
  field,
  inputProps,
  error,
  id,
}) => {
  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  const { key, ...props } = inputProps;

  return (
    <Select
      {...props}
      onValueChange={(value) => {
        const syntheticEvent = {
          target: {
            value,
            name: field.key,
          },
        } as React.ChangeEvent<HTMLInputElement>;
        props.onChange(syntheticEvent);
      }}
      defaultValue={field.default}
    >
      <SelectTrigger id={id} className={error ? "border-destructive" : ""}>
        <SelectValue placeholder="Select an option" />
      </SelectTrigger>
      <SelectContent>
        {(field.options || []).map(([key, label]) => (
          <SelectItem key={key} value={key}>
            {label}
          </SelectItem>
        ))}
      </SelectContent>
    </Select>
  );
};
