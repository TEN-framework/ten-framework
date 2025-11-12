//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import {
  type AutoFormUIComponents,
  AutoForm as BaseAutoForm,
} from "@autoform/react";
import { ArrayElementWrapper } from "./components/array-element-wrapper";
import { ArrayWrapper } from "./components/array-wrapper";
import { DateField } from "./components/date-field";
import { ErrorMessage } from "./components/error-message";
import { FieldWrapper } from "./components/field-wrapper";
import { Form } from "./components/form";
import { NumberField } from "./components/number-field";
import { ObjectWrapper } from "./components/object-wrapper";
import { SelectField } from "./components/select-field";
import { StringField } from "./components/string-field";
import { SubmitButton } from "./components/submit-button";
// import { BooleanField } from "./components/BooleanField";
import { SwitchField } from "./components/switch-field";
import type { AutoFormProps } from "./types";

const ShadcnUIComponents: AutoFormUIComponents = {
  Form,
  FieldWrapper,
  ErrorMessage,
  SubmitButton,
  ObjectWrapper,
  ArrayWrapper,
  ArrayElementWrapper,
};

export const ShadcnAutoFormFieldComponents = {
  string: StringField,
  number: NumberField,
  // boolean: BooleanField,
  boolean: SwitchField,
  date: DateField,
  select: SelectField,
} as const;
export type FieldTypes = keyof typeof ShadcnAutoFormFieldComponents;

export function AutoForm<T extends Record<string, unknown>>({
  uiComponents,
  formComponents,
  ...props
}: AutoFormProps<T>) {
  return (
    <BaseAutoForm
      {...props}
      uiComponents={{ ...ShadcnUIComponents, ...uiComponents }}
      formComponents={{ ...ShadcnAutoFormFieldComponents, ...formComponents }}
    />
  );
}
