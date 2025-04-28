//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import React, { useState, useEffect } from "react";
import { Switch } from "@/components/ui/Switch";
import { AutoFormFieldProps } from "@autoform/react";
import { Label } from "@/components/ui/Label";

export const SwitchField: React.FC<AutoFormFieldProps> = ({
  field,
  label,
  id,
  inputProps,
  value,
}) => {
  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  const { key, ...props } = inputProps;
  const [isChecked, setIsChecked] = useState<boolean>(value);

  useEffect(() => {
    setIsChecked(value);
  }, [value]);

  return (
    <div className="flex flex-col space-y-2">
      <Label htmlFor={id}>
        {label}
        {field.required && <span className="text-destructive"> *</span>}
      </Label>
      <Switch
        id={id}
        checked={isChecked}
        onCheckedChange={(change: boolean) => {
          setIsChecked(change);
          props.onChange({ target: { name: field.key, value: change } });
        }}
      />
    </div>
  );
};
