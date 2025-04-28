//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import React from "react";
import { Label } from "@/components/ui/Label";
import { FieldWrapperProps } from "@autoform/react";

const DISABLED_LABELS = ["boolean", "object", "array"];

export const FieldWrapper: React.FC<FieldWrapperProps> = ({
  label,
  children,
  id,
  field,
  error,
}) => {
  const isDisabled = DISABLED_LABELS.includes(field.type);

  return (
    <div className="space-y-2">
      {!isDisabled && (
        <Label htmlFor={id}>
          {label}
          {field.required && <span className="text-destructive"> *</span>}
        </Label>
      )}
      {children}
      {field.fieldConfig?.description && (
        <p className="text-sm text-muted-foreground">
          {field.fieldConfig.description}
        </p>
      )}
      {error && <p className="text-sm text-destructive">{error}</p>}
    </div>
  );
};
