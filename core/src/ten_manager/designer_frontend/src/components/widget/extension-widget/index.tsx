//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

// eslint-disable-next-line max-len
import { ExtensionDetails } from "@/components/widget/extension-widget/extension-details";
import type { IListTenCloudStorePackage } from "@/types/extension";

export const ExtensionWidget = (props: {
  className?: string;
  versions: IListTenCloudStorePackage[];
  name: string;
}) => {
  const { className, versions, name } = props;

  if (versions?.length === 0) {
    return null;
  }

  return (
    <ExtensionDetails versions={versions} name={name} className={className} />
  );
};
