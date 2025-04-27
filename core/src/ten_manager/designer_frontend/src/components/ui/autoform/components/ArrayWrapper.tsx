//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import React from "react";
import { Button } from "@/components/ui/Button";
import { PlusIcon } from "lucide-react";
import { ArrayWrapperProps } from "@autoform/react";

export const ArrayWrapper: React.FC<ArrayWrapperProps> = ({
  label,
  children,
  onAddItem,
}) => {
  return (
    <div className="space-y-4">
      <h3 className="text-lg font-medium">{label}</h3>
      {children}
      <Button onClick={onAddItem} variant="outline" size="sm" type="button">
        <PlusIcon className="h-4 w-4" />
      </Button>
    </div>
  );
};
