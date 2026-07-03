# Dynamic Fields for AutoForm

This feature allows dynamically adding custom fields to existing schemas in the AutoForm component, supporting up to 2 levels of nesting depth.

## Features

- ✅ Support for root-level dynamic field addition
- ✅ Support for dynamic fields in 1-level nested objects (e.g., `params.model_name`)
- ✅ Support for dynamic fields in 2-level nested objects (e.g., `config.audio.custom_field`)
- ✅ Support for three field types: `string`, `number`, `object`
- ✅ `object` type fields use JSON string input
- ✅ Dynamic fields do not override existing schema-defined fields
- ✅ Provides independent Tab page for managing dynamic fields

## Usage

### Basic Usage

```tsx
import { AutoForm } from "@/components/ui/autoform/auto-form";
import { z } from "zod";
import { ZodProvider } from "@autoform/zod";

const schema = z.object({
  dump: z.string().optional(),
  dump_path: z.string().optional(),
  params: z.object({
    id: z.string().optional(),
  }).optional(),
});

function MyComponent() {
  const handleDynamicFieldsChange = (fields) => {
    console.log("Dynamic fields changed:", fields);
  };

  return (
    <AutoForm
      schema={new ZodProvider(schema)}
      onSubmit={(data) => console.log(data)}
      allowDynamicFields={true}
      onDynamicFieldsChange={handleDynamicFieldsChange}
      dynamicFieldsTitle="Node Properties"
      withSubmit
    />
  );
}
```

### API

| Property | Type | Required | Default | Description |
|----------|------|----------|---------|-------------|
| `allowDynamicFields` | `boolean` | No | `false` | Enable dynamic fields feature |
| `onDynamicFieldsChange` | `(fields: TDynamicField[]) => void` | No | - | Callback when dynamic fields change |
| `dynamicFieldsTitle` | `string` | No | `"Properties"` | Title of the form Tab |

### TDynamicField Type

```typescript
export type TDynamicFieldType = "string" | "number" | "object";

export interface TDynamicField {
  key: string;                // Field name
  type: TDynamicFieldType;    // Field type
  value: unknown;             // Default value
  path: string[];             // Field path, [] for root level, ["params"] for under params
}
```

## Usage Examples

### 1. Root Level Field

Add a `dump_size` field at the same level as `dump` and `dump_path`:

- Path: Root Level
- Field name: `dump_size`
- Type: `number`

### 2. Nested Field (1 level)

Add a `model_name` field under the `params` object:

- Path: `params`
- Field name: `model_name`
- Type: `string`

### 3. Deep Nested Field (2 levels)

Add a `custom_settings` field under the `config.audio` object:

- Path: `config.audio`
- Field name: `custom_settings`
- Type: `object`
- Value: `{"key": "value"}` (JSON string)

## Important Notes

1. **Won't override existing fields**: If the schema already defines `params.id`, dynamic fields cannot redefine `params.id`
2. **Maximum 2 levels depth**: Supported path formats are `[]`, `["level1"]`, `["level1", "level2"]`
3. **Object type input**: When selecting object type, valid JSON string input is required
4. **Tab switching**: Dynamic fields are managed in an independent "Dynamic Fields" Tab. After definition, switch to the original form Tab to view the effect

## Implementation Details

- Dynamic fields are implemented by extending the original Zod schema
- Uses React.useMemo and React.useEffect to manage dynamic schema merging
- Supports both ZodProvider and direct Zod schema input formats
- Provides field conflict detection and user-friendly error messages
