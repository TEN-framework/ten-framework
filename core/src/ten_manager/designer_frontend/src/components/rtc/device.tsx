//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import type { TDeviceSelectItem } from "@/types/rtc";

export const DeviceSelect = (props: {
  items: TDeviceSelectItem[];
  value: string;
  onChange: (value: string) => void;
  placeholder?: string;
}) => {
  const { items, value, onChange, placeholder } = props;

  return (
    <Select value={value} onValueChange={onChange}>
      <SelectTrigger className="w-[290px]">
        <SelectValue placeholder={placeholder} />
      </SelectTrigger>
      <SelectContent>
        {items.map((item) => (
          <SelectItem key={item.value} value={item.value}>
            {item.label}
          </SelectItem>
        ))}
      </SelectContent>
    </Select>
  );
};
