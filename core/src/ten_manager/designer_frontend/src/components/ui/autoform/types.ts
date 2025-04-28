//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { ExtendableAutoFormProps } from "@autoform/react";
import { FieldValues } from "react-hook-form";

// eslint-disable-next-line @typescript-eslint/no-empty-object-type
export interface AutoFormProps<T extends FieldValues>
  extends ExtendableAutoFormProps<T> {}
