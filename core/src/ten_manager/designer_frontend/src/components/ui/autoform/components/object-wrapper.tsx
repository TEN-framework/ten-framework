//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import type { ObjectWrapperProps } from "@autoform/react";
import type React from "react";

export const ObjectWrapper: React.FC<ObjectWrapperProps> = ({
  label,
  children,
}) => {
  return (
    <div className="space-y-4">
      <h3 className="font-medium text-lg">{label}</h3>
      {children}
    </div>
  );
};
