//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import { Plus, X } from "lucide-react";
import React from "react";
import { useTranslation } from "react-i18next";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import { Separator } from "@/components/ui/separator";
import { Textarea } from "@/components/ui/textarea";
import type { TDynamicField, TDynamicFieldType } from "../../types";

interface DynamicFieldsPanelProps {
  dynamicFields: TDynamicField[];
  onChange: (fields: TDynamicField[]) => void;
  availablePaths: string[][];
}

export function DynamicFieldsPanel({
  dynamicFields,
  onChange,
  availablePaths,
}: DynamicFieldsPanelProps) {
  const [newField, setNewField] = React.useState<{
    key: string;
    type: TDynamicFieldType;
    value: string;
    path: string[];
  }>({
    key: "",
    type: "string",
    value: "",
    path: [],
  });

  const { t } = useTranslation();

  const pathOptions = React.useMemo(() => {
    return availablePaths.map((path) => ({
      value: path.length === 0 ? "__root__" : path.join("."),
      label: path.length === 0 ? t("ui.autoForm.rootLevel") : path.join("."),
      path: path,
    }));
  }, [availablePaths, t]);

  const convertValueByType = React.useCallback(
    (type: TDynamicFieldType, valueStr: string): unknown => {
      switch (type) {
        case "string":
          return valueStr;
        case "number": {
          const num = Number(valueStr);
          return Number.isNaN(num) ? 0 : num;
        }
        case "object": {
          if (!valueStr.trim()) return {};
          try {
            return JSON.parse(valueStr);
          } catch {
            return {};
          }
        }
        default:
          return valueStr;
      }
    },
    []
  );

  const isFieldKeyExists = React.useCallback(
    (key: string, path: string[]): boolean => {
      return dynamicFields.some(
        (field) =>
          field.key === key &&
          field.path.length === path.length &&
          field.path.every((p, i) => p === path[i])
      );
    },
    [dynamicFields]
  );

  const handleAddField = React.useCallback(() => {
    if (!newField.key.trim()) return;

    // Check if field already exists at this path
    if (isFieldKeyExists(newField.key, newField.path)) {
      alert(`Field "${newField.key}" already exists at this path!`);
      return;
    }

    const fieldValue = convertValueByType(newField.type, newField.value);

    const field: TDynamicField = {
      key: newField.key,
      type: newField.type,
      value: fieldValue,
      path: newField.path,
    };

    onChange([...dynamicFields, field]);

    // Reset form
    setNewField({
      key: "",
      type: "string",
      value: "",
      path: [],
    });
  }, [newField, dynamicFields, onChange, convertValueByType, isFieldKeyExists]);

  const handleRemoveField = React.useCallback(
    (index: number) => {
      const updatedFields = dynamicFields.filter((_, i) => i !== index);
      onChange(updatedFields);
    },
    [dynamicFields, onChange]
  );

  const handleUpdateField = React.useCallback(
    (index: number, updatedField: Partial<TDynamicField>) => {
      const updatedFields = dynamicFields.map((field, i) => {
        if (i === index) {
          const newFieldData = { ...field, ...updatedField };
          if (updatedField.type && updatedField.value !== undefined) {
            // Convert value when type changes
            const valueStr =
              typeof updatedField.value === "string"
                ? updatedField.value
                : JSON.stringify(updatedField.value);
            newFieldData.value = convertValueByType(
              updatedField.type,
              valueStr
            );
          }
          return newFieldData;
        }
        return field;
      });
      onChange(updatedFields);
    },
    [dynamicFields, onChange, convertValueByType]
  );

  const handlePathChange = React.useCallback((pathStr: string) => {
    const path = pathStr === "__root__" ? [] : pathStr.split(".");
    setNewField((prev) => ({ ...prev, path }));
  }, []);

  const renderValueInput = React.useCallback(
    (
      type: TDynamicFieldType,
      value: unknown,
      onChange: (value: string) => void,
      placeholder?: string
    ) => {
      const stringValue =
        typeof value === "string" ? value : JSON.stringify(value || "");

      switch (type) {
        case "string":
          return (
            <Input
              value={stringValue}
              onChange={(e) => onChange(e.target.value)}
              placeholder={placeholder || "Enter string value"}
            />
          );
        case "number":
          return (
            <Input
              type="number"
              value={stringValue}
              onChange={(e) => onChange(e.target.value)}
              placeholder={placeholder || "Enter number value"}
            />
          );
        case "object":
          return (
            <Textarea
              value={stringValue}
              onChange={(e) => onChange(e.target.value)}
              placeholder={
                placeholder || 'Enter JSON object (e.g., {"key": "value"})'
              }
              className="font-mono text-sm"
              rows={3}
            />
          );
        default:
          return (
            <Input
              value={stringValue}
              onChange={(e) => onChange(e.target.value)}
              placeholder={placeholder}
            />
          );
      }
    },
    []
  );

  return (
    <div className="space-y-6">
      {/* Add New Field Form */}
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            <Plus className="size-4" />
            {t("ui.autoForm.addDynamicField")}
          </CardTitle>
        </CardHeader>
        <CardContent className="space-y-4">
          {/* Path Selection */}
          <div className="space-y-2">
            <Label>{t("ui.autoForm.location")}</Label>
            <Select
              value={
                newField.path.length === 0
                  ? "__root__"
                  : newField.path.join(".")
              }
              onValueChange={handlePathChange}
            >
              <SelectTrigger>
                <SelectValue placeholder={t("ui.autoForm.selectLocation")} />
              </SelectTrigger>
              <SelectContent>
                {pathOptions.map((option) => (
                  <SelectItem key={option.value} value={option.value}>
                    {option.label}
                  </SelectItem>
                ))}
              </SelectContent>
            </Select>
          </div>

          {/* Field Key */}
          <div className="space-y-2">
            <Label>{t("ui.autoForm.fieldName")}</Label>
            <Input
              value={newField.key}
              onChange={(e) =>
                setNewField((prev) => ({ ...prev, key: e.target.value }))
              }
              placeholder={t("ui.autoForm.enterFieldName")}
            />
          </div>

          {/* Field Type */}
          <div className="space-y-2">
            <Label>{t("ui.autoForm.fieldType")}</Label>
            <Select
              value={newField.type}
              onValueChange={(value: TDynamicFieldType) =>
                setNewField((prev) => ({ ...prev, type: value, value: "" }))
              }
            >
              <SelectTrigger>
                <SelectValue />
              </SelectTrigger>
              <SelectContent>
                <SelectItem value="string">
                  {t("ui.autoForm.string")}
                </SelectItem>
                <SelectItem value="number">
                  {t("ui.autoForm.number")}
                </SelectItem>
                <SelectItem value="object">
                  {t("ui.autoForm.object")}
                </SelectItem>
              </SelectContent>
            </Select>
          </div>

          {/* Field Value */}
          <div className="space-y-2">
            <Label>{t("ui.autoForm.defaultValue")}</Label>
            {renderValueInput(
              newField.type,
              newField.value,
              (value) => setNewField((prev) => ({ ...prev, value })),
              t("ui.autoForm.enterDefaultValue")
            )}
          </div>

          <Button
            onClick={handleAddField}
            disabled={!newField.key.trim()}
            className="w-full"
          >
            <Plus className="mr-2 size-4" />
            {t("ui.autoForm.addField")}
          </Button>
        </CardContent>
      </Card>

      {/* Existing Dynamic Fields */}
      {dynamicFields.length > 0 && (
        <Card>
          <CardHeader>
            <CardTitle>
              {t("ui.autoForm.dynamicFields")} ({dynamicFields.length})
            </CardTitle>
          </CardHeader>
          <CardContent>
            <div className="space-y-4">
              {dynamicFields.map((field, index) => (
                <div key={`${field.path.join(".")}-${field.key}`}>
                  <div className="flex items-start gap-4 rounded-lg border p-4">
                    <div className="flex-1 space-y-3">
                      <div className="flex items-center gap-4">
                        <div className="flex-1">
                          <Label className="font-medium text-sm">
                            {t("ui.autoForm.path")}{" "}
                            {field.path.length === 0
                              ? t("ui.autoForm.root")
                              : field.path.join(".")}
                          </Label>
                          <div className="mt-1 text-muted-foreground text-xs">
                            {field.key} ({field.type})
                          </div>
                        </div>
                        <Button
                          variant="ghost"
                          size="sm"
                          onClick={() => handleRemoveField(index)}
                          className="text-destructive hover:text-destructive"
                        >
                          <X className="size-4" />
                        </Button>
                      </div>

                      <div className="grid grid-cols-1 gap-3 md:grid-cols-2">
                        <div className="space-y-2">
                          <Label>{t("ui.autoForm.fieldName")}</Label>
                          <Input
                            value={field.key}
                            onChange={(e) =>
                              handleUpdateField(index, { key: e.target.value })
                            }
                          />
                        </div>
                        <div className="space-y-2">
                          <Label>{t("ui.autoForm.type")}</Label>
                          <Select
                            value={field.type}
                            onValueChange={(value: TDynamicFieldType) => {
                              const newValue =
                                value === "object"
                                  ? {}
                                  : value === "number"
                                    ? 0
                                    : "";
                              handleUpdateField(index, {
                                type: value,
                                value: newValue,
                              });
                            }}
                          >
                            <SelectTrigger>
                              <SelectValue />
                            </SelectTrigger>
                            <SelectContent>
                              <SelectItem value="string">
                                {t("ui.autoForm.string")}
                              </SelectItem>
                              <SelectItem value="number">
                                {t("ui.autoForm.number")}
                              </SelectItem>
                              <SelectItem value="object">
                                {t("ui.autoForm.object")}
                              </SelectItem>
                            </SelectContent>
                          </Select>
                        </div>
                      </div>

                      <div className="space-y-2">
                        <Label>{t("ui.autoForm.defaultValue")}</Label>
                        {renderValueInput(field.type, field.value, (value) =>
                          handleUpdateField(index, { value })
                        )}
                      </div>
                    </div>
                  </div>
                  {index < dynamicFields.length - 1 && (
                    <Separator className="mt-4" />
                  )}
                </div>
              ))}
            </div>
          </CardContent>
        </Card>
      )}

      {dynamicFields.length === 0 && (
        <div className="rounded-lg border border-dashed p-8 text-center">
          <div className="text-muted-foreground">
            {t("ui.autoForm.noDynamicFields")}
          </div>
        </div>
      )}
    </div>
  );
}
