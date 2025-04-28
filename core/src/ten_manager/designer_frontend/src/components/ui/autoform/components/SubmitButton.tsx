//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import React from "react";
import { Button } from "@/components/ui/Button";

export const SubmitButton: React.FC<{ children: React.ReactNode }> = ({
  children,
}) => <Button type="submit">{children}</Button>;
