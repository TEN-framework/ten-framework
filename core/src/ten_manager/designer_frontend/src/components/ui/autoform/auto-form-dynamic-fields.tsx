//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

/* eslint-disable max-len */

import {
  type AutoFormUIComponents,
  AutoForm as BaseAutoForm,
} from "@autoform/react";
import { ZodProvider } from "@autoform/zod";
import React from "react";
import { useTranslation } from "react-i18next";
import { z } from "zod";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { ArrayElementWrapper } from "./components/array-element-wrapper";
import { ArrayWrapper } from "./components/array-wrapper";
import { DateField } from "./components/date-field";
import { DynamicFieldsPanel } from "./components/dynamic-fields/dynamic-fields-panel";
import { convertExtensionPropertySchema2ZodSchemaWithDynamicFields } from "./components/dynamic-fields/utils";
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
import type { AutoFormProps, TDynamicField } from "./types";

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

export function AutoFormDynamicFields<T extends Record<string, unknown>>({
  uiComponents,
  formComponents,
  allowDynamicFields = false,
  onDynamicFieldsChange,
  dynamicFieldsTitle = "Properties",
  originalSchema,
  ...props
}: AutoFormProps<T>) {
  const [dynamicFields, setDynamicFields] = React.useState<TDynamicField[]>([]);
  const [enhancedSchema, setEnhancedSchema] = React.useState(props.schema);

  const { t } = useTranslation();

  // Rebuild enhanced schema when dynamic fields or original schema changes
  React.useEffect(() => {
    if (originalSchema && allowDynamicFields) {
      console.log(
        "🔄 Rebuilding enhanced schema with dynamic fields:",
        dynamicFields
      );

      const propertySchemaEntries =
        convertExtensionPropertySchema2ZodSchemaWithDynamicFields(
          originalSchema,
          dynamicFields
        );

      const newEnhancedSchema = new ZodProvider(
        z.object(Object.fromEntries(propertySchemaEntries))
      );

      setEnhancedSchema(newEnhancedSchema);
    }
  }, [dynamicFields, originalSchema, allowDynamicFields]);

  // Extract available paths from schema
  const getAvailablePathsFromSchema = React.useCallback(
    (schemaToAnalyze: unknown): string[][] => {
      const paths: string[][] = [];
      paths.push([]); // Root path

      console.log("🔍 Analyzing schema for paths:", schemaToAnalyze);

      // Check if it's a ZodProvider
      if (
        schemaToAnalyze &&
        typeof schemaToAnalyze === "object" &&
        "parseSchema" in schemaToAnalyze
      ) {
        const provider = schemaToAnalyze as { parseSchema: () => unknown };
        const parsedSchema = provider.parseSchema();

        console.log("📦 Parsed schema from ZodProvider:", parsedSchema);

        if (
          parsedSchema &&
          typeof parsedSchema === "object" &&
          "fields" in parsedSchema
        ) {
          const fields = (
            parsedSchema as {
              fields: Array<{
                key: string;
                type: string;
                fields?: Array<{
                  key: string;
                  type: string;
                  fields?: unknown[];
                }>;
              }>;
            }
          ).fields;

          fields.forEach((field) => {
            console.log(`🔍 Processing field: ${field.key} (${field.type})`);

            if (field.type === "object") {
              // For object types, add their path
              paths.push([field.key]);
              console.log(`📁 Added path: [${field.key}]`);

              // Check for nested fields
              if (field.fields && Array.isArray(field.fields)) {
                field.fields.forEach((subField) => {
                  const fieldPath = `${field.key}.${subField.key}`;
                  console.log(
                    `🔍 Processing nested: ${fieldPath} (${subField.type})`
                  );

                  if (subField.type === "object") {
                    paths.push([field.key, subField.key]);
                    console.log(`📁📁 Added: [${field.key}, ${subField.key}]`);
                  }
                });
              }
            }
          });
        }
      }

      console.log("✅ Final available paths:", paths);
      return paths;
    },
    []
  );

  const availablePaths = React.useMemo(() => {
    // Use original schema or passed schema
    const schemaToUse = originalSchema ? props.schema : enhancedSchema;
    return getAvailablePathsFromSchema(schemaToUse);
  }, [
    originalSchema,
    props.schema,
    enhancedSchema,
    getAvailablePathsFromSchema,
  ]);

  const handleDynamicFieldsChange = React.useCallback(
    (fields: TDynamicField[]) => {
      console.log("Dynamic fields changed:", fields);
      setDynamicFields(fields);
      onDynamicFieldsChange?.(fields);
    },
    [onDynamicFieldsChange]
  );

  if (!allowDynamicFields) {
    return (
      <BaseAutoForm
        {...props}
        uiComponents={{ ...ShadcnUIComponents, ...uiComponents }}
        formComponents={{ ...ShadcnAutoFormFieldComponents, ...formComponents }}
      />
    );
  }

  return (
    <Tabs defaultValue="properties" className="w-full">
      <TabsList className="mb-4">
        <TabsTrigger value="properties">{dynamicFieldsTitle}</TabsTrigger>
        <TabsTrigger value="dynamic">
          {t("ui.autoForm.dynamicFields")}
        </TabsTrigger>
      </TabsList>

      <TabsContent value="properties" className="mt-0">
        <BaseAutoForm
          {...props}
          schema={enhancedSchema}
          uiComponents={{ ...ShadcnUIComponents, ...uiComponents }}
          formComponents={{
            ...ShadcnAutoFormFieldComponents,
            ...formComponents,
          }}
        />
        <div className="mt-4">
          <details className="group">
            <summary className="cursor-pointer font-medium text-muted-foreground text-sm hover:text-foreground">
              View Form Values (JSON)
            </summary>
            <pre className="mt-2 max-h-64 overflow-auto rounded-md bg-muted p-3 text-xs">
              {JSON.stringify(props.values || {}, null, 2)}
            </pre>
          </details>
        </div>
      </TabsContent>

      <TabsContent value="dynamic" className="mt-0">
        <DynamicFieldsPanel
          dynamicFields={dynamicFields}
          onChange={handleDynamicFieldsChange}
          availablePaths={availablePaths}
        />
      </TabsContent>
    </Tabs>
  );
}
