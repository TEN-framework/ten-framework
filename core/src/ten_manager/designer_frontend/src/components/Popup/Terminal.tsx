//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import TerminalWidget from "@/components/Widget/TerminalWidget";
import { ITerminalWidget } from "@/types/widgets";

export const TerminalPopupContent = (props: { widget: ITerminalWidget }) => {
  const { widget } = props;

  return <TerminalWidget id={widget.widget_id} data={widget.metadata} />;
};
