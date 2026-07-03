//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

// eslint-disable-next-line max-len
import { ExtensionDetails } from "@/components/widget/extension-widget/extension-details";
import type { ETenPackageType } from "@/types/extension";

export const ExtensionWidget = (props: {
  className?: string;
  name: string;
  type: ETenPackageType;
}) => {
  const { className, name, type } = props;

  return <ExtensionDetails name={name} className={className} type={type} />;
};
