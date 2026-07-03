//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { LoaderCircleIcon } from "lucide-react";

import { cn } from "@/lib/utils";

export function SpinnerLoading(props: {
  className?: string;
  svgProps?: React.SVGProps<SVGSVGElement>;
}) {
  const { className, svgProps } = props;
  const { className: svgClassName, ...rest } = svgProps ?? {};

  return (
    <div className={cn("flex items-center justify-center", className)}>
      <LoaderCircleIcon
        className={cn("animate-spin", svgClassName)}
        {...rest}
      />
    </div>
  );
}
