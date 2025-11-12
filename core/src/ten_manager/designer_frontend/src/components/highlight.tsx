//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

/** biome-ignore-all lint/suspicious/noArrayIndexKey: <ignore> */

export const HighlightText = (props: {
  children: React.ReactNode | string;
  highlight?: string;
  className?: string;
}) => {
  const { children, highlight, className } = props;

  if (!highlight) {
    return <span className={className}>{children}</span>;
  }

  const escapeRegExp = (str: string) =>
    str.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  const escapedHighlight = escapeRegExp(highlight);
  const parts = String(children).split(
    new RegExp(`(${escapedHighlight})`, "gi")
  );

  return (
    <span className={className}>
      {parts.map((part, index) =>
        part.toLowerCase() === highlight.toLowerCase() ? (
          <mark
            key={`highlight-${part}-${index}`}
            className="bg-yellow-200 dark:bg-yellow-900"
          >
            {part}
          </mark>
        ) : (
          part
        )
      )}
    </span>
  );
};
