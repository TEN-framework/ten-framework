//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import React from "react";

export const Form = React.forwardRef<
  HTMLFormElement,
  React.ComponentProps<"form">
>(({ children, ...props }, ref) => {
  return (
    <form ref={ref} className="space-y-4" {...props}>
      {children}
    </form>
  );
});
