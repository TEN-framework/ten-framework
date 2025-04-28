//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
/* eslint-disable max-len */
import { useTheme } from "next-themes";
import { Toaster as Sonner } from "sonner";
import {
  TriangleAlertIcon,
  CircleCheckIcon,
  InfoIcon,
  LoaderCircleIcon,
  CircleXIcon,
} from "lucide-react";

type ToasterProps = React.ComponentProps<typeof Sonner>;

const Toaster = ({ ...props }: ToasterProps) => {
  const { theme = "system" } = useTheme();

  return (
    <Sonner
      theme={theme as ToasterProps["theme"]}
      className="toaster group"
      toastOptions={{
        classNames: {
          toast:
            "group toast group-[.toaster]:bg-background group-[.toaster]:text-foreground group-[.toaster]:border-border group-[.toaster]:shadow-lg",
          description: "group-[.toast]:text-muted-foreground",
          actionButton:
            "group-[.toast]:bg-primary group-[.toast]:text-primary-foreground",
          cancelButton:
            "group-[.toast]:bg-muted group-[.toast]:text-muted-foreground",
        },
      }}
      icons={{
        success: <CircleCheckIcon className="h-4 w-4 text-green-500" />,
        info: <InfoIcon className="h-4 w-4 text-blue-500" />,
        warning: <TriangleAlertIcon className="h-4 w-4 text-amber-500" />,
        error: <CircleXIcon className="h-4 w-4 text-red-500" />,
        loading: (
          <LoaderCircleIcon className="h-4 w-4 text-gray-500 animate-spin" />
        ),
      }}
      {...props}
    />
  );
};

export { Toaster };
