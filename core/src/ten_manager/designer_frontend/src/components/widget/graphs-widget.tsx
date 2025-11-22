//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import { ZodProvider } from "@autoform/zod";
import { zodResolver } from "@hookform/resolvers/zod";
import { ArrowBigRightIcon, EditIcon } from "lucide-react";
import * as React from "react";
import { useForm } from "react-hook-form";
import { useTranslation } from "react-i18next";
import { toast } from "sonner";
import { z } from "zod";
import { useFetchAddons } from "@/api/services/addons";
import {
  retrieveExtensionDefaultProperty,
  retrieveExtensionSchema,
  useFetchExtSchema,
} from "@/api/services/extension";
import {
  postAddConnection,
  postAddNode,
  postReplaceNode,
  postUpdateNodeProperty,
  useGraphs,
} from "@/api/services/graphs";
import { useCompatibleMessages } from "@/api/services/messages";
import { SpinnerLoading } from "@/components/status/loading";
// eslint-disable-next-line max-len
import { AutoFormDynamicFields } from "@/components/ui/autoform/auto-form-dynamic-fields";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { Combobox, MultiSelectorWithCheckbox } from "@/components/ui/combobox";
import {
  Form,
  FormControl,
  // FormDescription,
  FormField,
  FormItem,
  FormLabel,
  FormMessage,
} from "@/components/ui/form";
import { Input } from "@/components/ui/input";
import {
  Select,
  SelectContent,
  SelectGroup,
  SelectItem,
  SelectLabel,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import {
  convertExtensionPropertySchema2ZodSchema,
  type TExtPropertySchema,
} from "@/components/widget/utils";
import { cn } from "@/lib/utils";
import { useDialogStore, useFlowStore } from "@/store";
import { ECustomNodeType, type TCustomNode } from "@/types/flow";
import {
  AddConnectionPayloadSchema,
  AddNodePayloadSchema,
  EConnectionType,
  EMsgDirection,
  UpdateNodePropertyPayloadSchema,
} from "@/types/graphs";

const GraphAddNodePropertyField = (props: {
  base_dir?: string | null;
  addon: string;
  onChange?: (value: Record<string, unknown> | undefined) => void;
}) => {
  const { base_dir, addon, onChange } = props;

  const [isLoading, setIsLoading] = React.useState(false);
  const [errMsg, setErrMsg] = React.useState<string | null>(null);
  const [propertySchemaEntries, setPropertySchemaEntries] = React.useState<
    [string, z.ZodType][]
  >([]);
  const [defaultProperty, setDefaultProperty] = React.useState<
    Record<string, unknown> | undefined | null
  >(null);

  const { t } = useTranslation();
  const { appendDialog, removeDialog } = useDialogStore();

  const isSchemaEmptyMemo = React.useMemo(() => {
    return !isLoading && propertySchemaEntries.length === 0;
  }, [isLoading, propertySchemaEntries.length]);

  // biome-ignore lint/correctness/useExhaustiveDependencies: <ignore>
  React.useEffect(() => {
    const init = async () => {
      try {
        setIsLoading(true);

        const addonSchema = await retrieveExtensionSchema({
          appBaseDir: base_dir ?? "",
          addonName: addon,
        });
        const propertySchema = addonSchema.property?.properties;
        if (!propertySchema) {
          // toast.error(t("popup.graph.noPropertySchema"));
          return;
        }
        const defaultProperty = await retrieveExtensionDefaultProperty({
          appBaseDir: base_dir ?? "",
          addonName: addon,
        });
        if (defaultProperty) {
          setDefaultProperty(defaultProperty);
          onChange?.(defaultProperty);
        }
        const propertySchemaEntries =
          convertExtensionPropertySchema2ZodSchema(propertySchema);
        setPropertySchemaEntries(propertySchemaEntries);
      } catch (error) {
        console.error(error);
        if (error instanceof Error) {
          setErrMsg(error.message);
        } else {
          setErrMsg(t("popup.default.errorUnknown"));
        }
      } finally {
        setIsLoading(false);
      }
    };

    init();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const handleClick = (e: React.MouseEvent<HTMLButtonElement>) => {
    e.preventDefault();

    const dialogId = `new-node-property`;
    appendDialog({
      id: dialogId,
      title: t("popup.graph.property"),
      content: (
        <>
          <AutoFormDynamicFields
            onSubmit={async (data: Record<string, unknown>) => {
              onChange?.(data);
              removeDialog(dialogId);
            }}
            defaultValues={defaultProperty || undefined}
            schema={
              new ZodProvider(
                z.object(Object.fromEntries(propertySchemaEntries))
              )
            }
          >
            <div className="flex w-full flex-row justify-end gap-2">
              <Button
                type="button"
                variant="outline"
                onClick={() => {
                  removeDialog(dialogId);
                }}
              >
                {t("action.cancel")}
              </Button>
              <Button type="submit">{t("action.confirm")}</Button>
            </div>
          </AutoFormDynamicFields>
        </>
      ),
    });
  };

  return (
    <div className="flex h-fit w-full flex-col gap-2">
      <Button
        variant="outline"
        disabled={isSchemaEmptyMemo || isLoading}
        onClick={handleClick}
      >
        {isLoading && <SpinnerLoading className="size-4" />}
        {!isLoading && <EditIcon className="size-4" />}
        {isSchemaEmptyMemo && <>{t("popup.graph.noPropertySchema")}</>}
        {t("action.edit")}
      </Button>
      {errMsg && <div className="text-red-500">{errMsg}</div>}
    </div>
  );
};

export const GraphAddNodeWidget = (props: {
  base_dir?: string | null;
  graph_id: string;
  postAddNodeActions?: () => void | Promise<void>;
  node?: TCustomNode | null;
  isReplaceNode?: boolean;
}) => {
  const {
    base_dir,
    graph_id,
    postAddNodeActions,
    node,
    isReplaceNode = false,
  } = props;
  const [customAddon, setCustomAddon] = React.useState<string | undefined>(
    undefined
  );
  const [isSubmitting, setIsSubmitting] = React.useState(false);
  const [remoteCheckErrorMessage, setRemoteCheckErrorMessage] = React.useState<
    string | undefined
  >(undefined);

  const { t } = useTranslation();

  const {
    data: graphs,
    isLoading: isGraphsLoading,
    error: graphError,
    mutate: mutateGraphs,
  } = useGraphs();
  const {
    data: addons,
    isLoading: isAddonsLoading,
    error: addonError,
  } = useFetchAddons({ base_dir: base_dir });

  const form = useForm<z.infer<typeof AddNodePayloadSchema>>({
    resolver: zodResolver(AddNodePayloadSchema),
    defaultValues: {
      graph_id: graph_id ?? node?.data?.graph?.graph_id ?? "",
      name: (node?.data?.name as string | undefined) || undefined,
      addon: undefined,
      extension_group: undefined,
      app: undefined,
      property: undefined,
    },
  });

  const onSubmit = async (data: z.infer<typeof AddNodePayloadSchema>) => {
    setIsSubmitting(true);
    try {
      if (isReplaceNode) {
        await postReplaceNode(data);
      } else {
        await postAddNode(data);
      }
      await mutateGraphs();
      postAddNodeActions?.();
      toast.success(
        isReplaceNode
          ? t("popup.graph.replaceNodeSuccess")
          : t("popup.graph.addNodeSuccess"),
        {
          description: `${data.name}`,
        }
      );
    } catch (error) {
      console.error(error);
      setRemoteCheckErrorMessage(
        error instanceof Error ? error.message : "Unknown error"
      );
    } finally {
      setIsSubmitting(false);
    }
  };

  const comboboxOptionsMemo = React.useMemo(() => {
    const addonsOptions = addons
      ? addons.map((addon) => ({
          value: addon.name,
          label: addon.name,
        }))
      : [];
    const customAddons = customAddon
      ? [{ value: customAddon, label: customAddon }]
      : [];
    return [...addonsOptions, ...customAddons];
  }, [addons, customAddon]);

  React.useEffect(() => {
    if (graphError) {
      toast.error(t("popup.graph.graphError"), {
        description: graphError.message,
      });
    }
    if (addonError) {
      toast.error(t("popup.graph.addonError"), {
        description: addonError.message,
      });
    }
  }, [graphError, addonError, t]);

  return (
    <Form {...form}>
      <form
        onSubmit={form.handleSubmit(onSubmit)}
        className="h-full w-full space-y-4 overflow-y-auto px-2"
      >
        <FormField
          control={form.control}
          name="graph_id"
          render={({ field }) => (
            <FormItem>
              <FormLabel>{t("popup.graph.graphId")}</FormLabel>
              <FormControl>
                <Select
                  onValueChange={field.onChange}
                  defaultValue={field.value}
                  disabled={isReplaceNode}
                >
                  <SelectTrigger className="w-full" disabled={isGraphsLoading}>
                    <SelectValue placeholder={t("popup.graph.graphId")} />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectGroup>
                      <SelectLabel>{t("popup.graph.graphId")}</SelectLabel>
                      {isGraphsLoading ? (
                        <SelectItem value={t("popup.graph.graphId")}>
                          <SpinnerLoading className="size-4" />
                        </SelectItem>
                      ) : (
                        graphs?.map((graph) => (
                          <SelectItem
                            key={graph.graph_id}
                            value={graph.graph_id}
                          >
                            {graph.name}
                          </SelectItem>
                        ))
                      )}
                    </SelectGroup>
                  </SelectContent>
                </Select>
              </FormControl>
              <FormMessage />
            </FormItem>
          )}
        />

        <FormField
          control={form.control}
          name="name"
          render={({ field }) => (
            <FormItem>
              <FormLabel>{t("popup.graph.nodeName")}</FormLabel>
              <FormControl>
                <Input
                  placeholder={t("popup.graph.nodeName")}
                  {...field}
                  disabled={field?.disabled || isReplaceNode}
                />
              </FormControl>
              <FormMessage />
            </FormItem>
          )}
        />

        <FormField
          control={form.control}
          name="addon"
          render={({ field }) => (
            <FormItem>
              <FormLabel>{t("popup.graph.addonName")}</FormLabel>
              <FormControl>
                <Combobox
                  options={comboboxOptionsMemo}
                  placeholder={t("popup.graph.addonName")}
                  selected={field.value}
                  onChange={(i) => {
                    field.onChange(i.value);
                  }}
                  onCreate={(i) => {
                    setCustomAddon(i);
                    field.onChange(i);
                  }}
                />
              </FormControl>
              <FormMessage />
            </FormItem>
          )}
        />

        {form.watch("addon") && (
          <FormField
            control={form.control}
            name="property"
            render={({ field }) => (
              <FormItem>
                <FormLabel>{t("popup.graph.property")}</FormLabel>
                <FormControl>
                  <GraphAddNodePropertyField
                    key={form.watch("addon")}
                    addon={form.watch("addon")}
                    onChange={(value: Record<string, unknown> | undefined) => {
                      field.onChange(value);
                    }}
                    base_dir={base_dir}
                  />
                </FormControl>
                <FormMessage />
              </FormItem>
            )}
          />
        )}

        {remoteCheckErrorMessage && (
          <div className="flex flex-col gap-2 text-red-500">
            <p>
              {isReplaceNode
                ? t("popup.graph.replaceNodeFailed")
                : t("popup.graph.addNodeFailed")}
            </p>
            <p>{remoteCheckErrorMessage}</p>
          </div>
        )}

        <Button
          type="submit"
          disabled={isAddonsLoading || isGraphsLoading || isSubmitting}
        >
          {isSubmitting ? (
            <SpinnerLoading className="size-4" />
          ) : isReplaceNode ? (
            t("popup.graph.replaceNode")
          ) : (
            t("popup.graph.addNode")
          )}
        </Button>
      </form>
    </Form>
  );
};

export const GraphUpdateNodePropertyWidget = (props: {
  base_dir?: string | null;
  app_uri?: string | null;
  graph_id: string;
  node: TCustomNode;
  postUpdateNodePropertyActions?: () => void | Promise<void>;
}) => {
  const { base_dir, app_uri, graph_id, node, postUpdateNodePropertyActions } =
    props;
  const [isSubmitting, setIsSubmitting] = React.useState(false);
  const [isSchemaLoading, setIsSchemaLoading] = React.useState(false);
  const [propertySchemaEntries, setPropertySchemaEntries] = React.useState<
    [string, z.ZodType][]
  >([]);
  const [originalSchema, setOriginalSchema] =
    React.useState<TExtPropertySchema>({});

  const { t } = useTranslation();

  const { mutate: mutateGraphs } = useGraphs();

  React.useEffect(() => {
    const fetchSchema = async () => {
      try {
        setIsSchemaLoading(true);
        const schema = await retrieveExtensionSchema({
          appBaseDir: base_dir ?? "",
          addonName: typeof node.data.addon === "string" ? node.data.addon : "",
        });

        // Store original schema for dynamic fields
        const originalSchemaProps = schema.property?.properties ?? {};
        setOriginalSchema(originalSchemaProps);

        const propertySchemaEntries =
          convertExtensionPropertySchema2ZodSchema(originalSchemaProps);
        setPropertySchemaEntries(propertySchemaEntries);
      } catch (error) {
        console.error(error);
        toast.error(error instanceof Error ? error.message : "Unknown error");
      } finally {
        setIsSchemaLoading(false);
      }
    };

    fetchSchema();
  }, [base_dir, node.data.addon]);

  return (
    <div className="h-full overflow-y-auto">
      {isSchemaLoading && !propertySchemaEntries && (
        <SpinnerLoading className="size-4" />
      )}
      {propertySchemaEntries?.length > 0 ? (
        <AutoFormDynamicFields
          values={node?.data.property || {}}
          schema={
            new ZodProvider(z.object(Object.fromEntries(propertySchemaEntries)))
          }
          originalSchema={originalSchema}
          allowDynamicFields={true}
          dynamicFieldsTitle="Node Properties"
          onSubmit={async (data: Record<string, unknown>) => {
            setIsSubmitting(true);
            try {
              const nodeData = UpdateNodePropertyPayloadSchema.parse({
                graph_id: graph_id ?? node?.data?.graph?.graph_id ?? "",
                name: node.data.name,
                addon: node.data.addon,
                extension_group: node.data.extension_group,
                app: app_uri ?? undefined,
                property: JSON.stringify(data, null, 2),
              });
              await postUpdateNodeProperty(nodeData);
              await mutateGraphs();
              toast.success(t("popup.graph.updateNodePropertySuccess"), {
                description: `${node.data.name}`,
              });
              postUpdateNodePropertyActions?.();
            } catch (error) {
              console.error(error);
              toast.error(t("popup.graph.updateNodePropertyFailed"), {
                description:
                  error instanceof Error ? error.message : "Unknown error",
              });
            } finally {
              setIsSubmitting(false);
            }
          }}
          withSubmit
          formProps={{
            className: cn(
              "flex h-full flex-col gap-4 overflow-y-auto px-1",
              isSubmitting && "disabled"
            ),
          }}
        />
      ) : (
        <div className="text-center text-gray-500 text-sm">
          {t("popup.graph.noPropertySchema")}
        </div>
      )}
    </div>
  );
};

// Main Connection Creation Widget - Routes to appropriate sub-widget
export const GraphConnectionCreationWidget = (props: {
  base_dir?: string | null;
  app_uri?: string | null;
  graph_id: string;
  src_node?: TCustomNode | null;
  dest_node?: TCustomNode | null;
  postAddConnectionActions?: () => void | Promise<void>;
}) => {
  const { src_node, dest_node } = props;

  // Determine connection type based on node types
  const srcType = src_node?.type;
  const destType = dest_node?.type;

  // Extension to Extension connection
  if (
    (!srcType || srcType === ECustomNodeType.EXTENSION) &&
    (!destType || destType === ECustomNodeType.EXTENSION)
  ) {
    return <ExtToExtConnectionWidget {...props} />;
  }

  // Selector to Extension connection
  if (
    srcType === ECustomNodeType.SELECTOR &&
    (!destType || destType === ECustomNodeType.EXTENSION)
  ) {
    return <SelectorToExtConnectionWidget {...props} />;
  }

  // Unsupported connection type
  return (
    <div className="p-4 text-center text-muted-foreground">
      Unsupported connection type
    </div>
  );
};

// Extension to Extension Connection Widget
const ExtToExtConnectionWidget = (props: {
  base_dir?: string | null;
  app_uri?: string | null;
  graph_id: string;
  src_node?: TCustomNode | null;
  dest_node?: TCustomNode | null;
  postAddConnectionActions?: () => void | Promise<void>;
}) => {
  const {
    base_dir,
    app_uri,
    graph_id,
    src_node,
    dest_node,
    postAddConnectionActions,
  } = props;
  const [isSubmitting, setIsSubmitting] = React.useState(false);
  const [msgNameList, setMsgNameList] = React.useState<
    {
      value: string;
      label: string;
      disabled?: boolean;
    }[]
  >([]);
  // Mode: 'src' means src-first mode, 'dest' means dest-first mode,
  // null means not determined yet
  const [mode, setMode] = React.useState<"src" | "dest" | null>(() => {
    if (src_node) return "src";
    if (dest_node) return "dest";
    return null;
  });

  const { t } = useTranslation();
  const { nodes } = useFlowStore();
  const {
    data: graphs,
    isLoading: isGraphsLoading,
    error: graphError,
    mutate: mutateGraphs,
  } = useGraphs();

  const form = useForm<z.infer<typeof AddConnectionPayloadSchema>>({
    resolver: zodResolver(AddConnectionPayloadSchema),
    defaultValues: {
      graph_id: graph_id ?? "",
      src: {
        app: app_uri,
        extension:
          src_node?.type === ECustomNodeType.EXTENSION &&
          typeof src_node?.data.name === "string"
            ? src_node.data.name
            : undefined,
      },
      dest: {
        app: app_uri,
        extension:
          dest_node?.type === ECustomNodeType.EXTENSION &&
          typeof dest_node?.data.name === "string"
            ? dest_node.data.name
            : undefined,
      },
      msg_names: [],
      msg_type: EConnectionType.CMD,
    },
  });

  const currentSrcExtension = form.watch("src.extension");
  const currentDestExtension = form.watch("dest.extension");
  const activeExtension =
    mode === "src"
      ? currentSrcExtension || src_node?.data.name
      : mode === "dest"
        ? currentDestExtension || dest_node?.data.name
        : currentSrcExtension ||
          currentDestExtension ||
          src_node?.data.name ||
          dest_node?.data.name;

  const {
    data: extSchema,
    isLoading: isExtSchemaLoading,
    error: extSchemaError,
  } = useFetchExtSchema(
    activeExtension
      ? {
          appBaseDir: base_dir ?? "",
          addonName: (src_node?.data.addon ||
            dest_node?.data.addon ||
            nodes.find((n) => n.data.name === activeExtension)?.data
              .addon) as string,
        }
      : null
  );

  const watchedMsgNames = form.watch("msg_names");
  const primaryMsgName = Array.isArray(watchedMsgNames)
    ? watchedMsgNames[0]
    : undefined;

  const onSubmit = async (data: z.infer<typeof AddConnectionPayloadSchema>) => {
    console.log("onSubmit === ", { src_node, dest_node, data });
    setIsSubmitting(true);
    try {
      const payload = AddConnectionPayloadSchema.parse(data);
      if (
        payload.src?.extension &&
        payload.src?.extension === payload.dest?.extension
      ) {
        throw new Error(t("popup.graph.sameNodeError"));
      }
      await postAddConnection(payload);
      await mutateGraphs();
      postAddConnectionActions?.();
    } catch (error) {
      toast.error(error instanceof Error ? error.message : "Unknown error");
      console.error(error);
    } finally {
      setIsSubmitting(false);
    }
  };

  const activeExtensionForCompatible =
    mode === "src"
      ? currentSrcExtension || src_node?.data.name
      : mode === "dest"
        ? currentDestExtension || dest_node?.data.name
        : currentSrcExtension || currentDestExtension;

  const activeExtensionGroup =
    mode === "src"
      ? src_node?.data.extension_group
      : mode === "dest"
        ? dest_node?.data.extension_group
        : src_node?.data.extension_group || dest_node?.data.extension_group;

  const msgType = form.watch("msg_type");
  const { data: compatibleMessages, error: compatibleMsgError } =
    useCompatibleMessages(
      activeExtensionForCompatible &&
        msgType &&
        primaryMsgName &&
        graph_id &&
        !(src_node && dest_node)
        ? {
            graph_id: graph_id ?? "",
            app: app_uri ?? undefined,
            extension_group: activeExtensionGroup as string | undefined,
            extension: activeExtensionForCompatible as string,
            msg_type: msgType,
            msg_direction:
              mode === "src" || (mode === null && currentSrcExtension)
                ? EMsgDirection.OUT
                : EMsgDirection.IN,
            msg_name: primaryMsgName,
          }
        : null
    );

  const compatibleMessagesExtList = React.useMemo(() => {
    if (!compatibleMessages) return [];
    return compatibleMessages.map((i) => i.extension);
  }, [compatibleMessages]);

  const [srcNodes, destNodes] = React.useMemo(() => {
    const allExtensionNodes = nodes.filter(
      (n) =>
        n.data.graph.graph_id === graph_id &&
        n.type === ECustomNodeType.EXTENSION
    );

    return allExtensionNodes.reduce(
      (prev, cur) => {
        // If src_node is fixed, only add matching nodes to srcNodes
        if (src_node && cur.data.name === src_node.data.name) {
          prev[0].push(cur);
          return prev;
        }
        // If dest_node is fixed, only add matching nodes to destNodes
        if (dest_node && cur.data.name === dest_node.data.name) {
          prev[1].push(cur);
          return prev;
        }
        // If both are fixed, don't add to either
        if (src_node && dest_node) {
          return prev;
        }
        // Otherwise, add to both lists (user can choose)
        prev[0].push(cur);
        prev[1].push(cur);
        return prev;
      },
      [[], []] as [TCustomNode[], TCustomNode[]]
    );
  }, [nodes, graph_id, src_node, dest_node]);

  React.useEffect(() => {
    const direction =
      mode === "src" || (mode === null && currentSrcExtension) ? "out" : "in";
    if (extSchema && activeExtension) {
      const srcMsgNameList =
        extSchema?.[`${msgType}_${direction}`]?.map((i) => i.name) ?? [];
      const newMsgNameList = [
        ...srcMsgNameList.map((i) => ({
          value: i,
          label: `${i}`,
        })),
      ];
      setMsgNameList(newMsgNameList);
    }
  }, [extSchema, msgType, mode, currentSrcExtension, activeExtension]);

  // biome-ignore lint/correctness/useExhaustiveDependencies: <ignore>
  React.useEffect(() => {
    if (graphError) {
      toast.error(t("popup.graph.graphError"), {
        description: graphError.message,
      });
    }
    if (extSchemaError) {
      toast.error(t("popup.graph.addonError"), {
        description:
          extSchemaError instanceof Error
            ? extSchemaError.message
            : "Unknown error",
      });
    }
    if (compatibleMsgError) {
      toast.error(t("popup.graph.addonError"), {
        description:
          compatibleMsgError instanceof Error
            ? compatibleMsgError.message
            : "Unknown error",
      });
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [graphError, extSchemaError, compatibleMsgError]);

  const MessageTypeAndNameFields = () => {
    return (
      <>
        <FormField
          control={form.control}
          name="msg_type"
          render={({ field }) => (
            <FormItem>
              <FormLabel>{t("popup.graph.messageType")}</FormLabel>
              <FormControl>
                <Select
                  onValueChange={(val) => {
                    field.onChange(val);
                    form.setValue("msg_names", []);
                    form.trigger("msg_names");
                  }}
                  value={field.value}
                >
                  <SelectTrigger className="w-full overflow-hidden">
                    <SelectValue placeholder={t("popup.graph.messageType")} />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectGroup>
                      <SelectLabel>{t("popup.graph.messageType")}</SelectLabel>
                      {Object.values(EConnectionType).map((type) => (
                        <SelectItem key={type} value={type}>
                          {type}
                        </SelectItem>
                      ))}
                    </SelectGroup>
                  </SelectContent>
                </Select>
              </FormControl>
              <FormMessage />
            </FormItem>
          )}
        />
        {form.watch("msg_type") && (
          <FormField
            control={form.control}
            name="msg_names"
            render={({ field }) => (
              <FormItem>
                <FormLabel>{t("popup.graph.messageName")}</FormLabel>
                <FormControl>
                  <Combobox
                    key={`Combobox-${form.watch("msg_type")}-${
                      form.watch("src").extension
                    }`}
                    disabled={isExtSchemaLoading}
                    isLoading={isExtSchemaLoading}
                    mode="multiple"
                    maxSelectedItems={2}
                    options={msgNameList}
                    placeholder={t("popup.graph.messageName")}
                    selected={field.value ?? []}
                    onChange={(items) => {
                      field.onChange(items.map((item) => item.value));
                    }}
                    onCreate={(i) => {
                      setMsgNameList((prev) => {
                        if (prev.some((item) => item.value === i)) {
                          return prev;
                        }

                        return [
                          ...prev,
                          {
                            value: i,
                            label: i,
                          },
                        ];
                      });
                      const currentValues = field.value ?? [];
                      field.onChange([...currentValues, i]);
                    }}
                  />
                </FormControl>
                <FormMessage />
              </FormItem>
            )}
          />
        )}
      </>
    );
  };

  return (
    <Form {...form}>
      <form
        onSubmit={form.handleSubmit(onSubmit)}
        className="h-full w-full space-y-4 overflow-y-auto px-2"
      >
        <FormField
          control={form.control}
          name="graph_id"
          render={({ field }) => (
            <FormItem>
              <FormLabel>{t("popup.graph.graphName")}</FormLabel>
              <FormControl>
                <Select
                  onValueChange={field.onChange}
                  defaultValue={field.value}
                  disabled={!!graph_id}
                >
                  <SelectTrigger className="w-full" disabled={isGraphsLoading}>
                    <SelectValue placeholder={t("popup.graph.graphName")} />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectGroup>
                      <SelectLabel>{t("popup.graph.graphName")}</SelectLabel>
                      {isGraphsLoading ? (
                        <SelectItem value={t("popup.graph.graphName")}>
                          <SpinnerLoading className="size-4" />
                        </SelectItem>
                      ) : (
                        graphs?.map((graph) => (
                          <SelectItem
                            key={graph.graph_id}
                            value={graph.graph_id}
                          >
                            {graph.name}
                          </SelectItem>
                        ))
                      )}
                    </SelectGroup>
                  </SelectContent>
                </Select>
              </FormControl>
              <FormMessage />
            </FormItem>
          )}
        />
        <div className="flex items-center justify-between gap-4">
          <div className="flex-1 space-y-4 rounded-md bg-muted/50 p-4">
            <FormField
              control={form.control}
              name="src"
              render={({ field }) => (
                <FormItem>
                  <FormLabel>{t("popup.graph.srcLocation")}</FormLabel>
                  <FormControl>
                    <Select
                      onValueChange={(val) => {
                        if (!val) return;
                        const previousMode = mode;
                        const previousSrcExtension = field.value.extension;
                        // Set mode to 'src' if not determined yet
                        if (mode === null && !src_node && !dest_node) {
                          setMode("src");
                        }
                        field.onChange({
                          app: app_uri ?? undefined,
                          extension: val,
                        });
                        // Only clear msg_names if:
                        // 1. Switching to src mode (from null or dest), OR
                        // 2. In src mode and changing src extension
                        // (different extension means different messages)
                        const isSwitchingToSrcMode =
                          previousMode !== "src" && mode === "src";
                        if (
                          isSwitchingToSrcMode ||
                          (previousMode === "src" &&
                            previousSrcExtension !== val)
                        ) {
                          form.setValue("msg_names", []);
                          form.trigger("msg_names");
                        }
                      }}
                      value={field.value.extension || undefined}
                      disabled={
                        src_node
                          ? true
                          : mode === null
                            ? false
                            : !!(
                                mode === "dest" && !(msgType && primaryMsgName)
                              )
                      }
                    >
                      <SelectTrigger
                        className={cn(
                          "w-full overflow-hidden",
                          "[&_.badge]:hidden"
                        )}
                      >
                        <SelectValue placeholder={t("popup.graph.srcLocation")}>
                          {field.value.extension}
                          {field.value.extension && " (extension)"}
                        </SelectValue>
                      </SelectTrigger>
                      <SelectContent>
                        <SelectGroup>
                          <SelectLabel>
                            {t("popup.graph.srcLocation")}
                          </SelectLabel>
                          {srcNodes
                            .filter(
                              (n) =>
                                n.id !== dest_node?.id &&
                                n.data.name !== currentDestExtension
                            )
                            .sort((a, b) => {
                              const aCompatible =
                                compatibleMessagesExtList.includes(
                                  a.data.addon as string
                                );
                              const bCompatible =
                                compatibleMessagesExtList.includes(
                                  b.data.addon as string
                                );
                              return aCompatible === bCompatible
                                ? 0
                                : aCompatible
                                  ? -1
                                  : 1;
                            })
                            .map((node) => (
                              <SelectItem
                                key={node.id}
                                value={node.data.name as string}
                              >
                                {node.data.name as string}{" "}
                                {compatibleMessagesExtList.includes(
                                  node.data.addon as string
                                ) && (
                                  <Badge
                                    className={cn(
                                      "badge",
                                      "bg-ten-green-6 hover:bg-ten-green-6"
                                    )}
                                  >
                                    {t("extensionStore.compatible")}
                                  </Badge>
                                )}
                              </SelectItem>
                            ))}
                        </SelectGroup>
                      </SelectContent>
                    </Select>
                  </FormControl>
                  <FormMessage />
                </FormItem>
              )}
            />
            {(!!src_node || (mode === "src" && currentSrcExtension)) && (
              <MessageTypeAndNameFields />
            )}
          </div>
          <ArrowBigRightIcon className="mx-auto size-4" />
          <div className="flex-1 space-y-4 rounded-md bg-muted/50 p-4">
            <FormField
              control={form.control}
              name="dest"
              render={({ field }) => (
                <FormItem>
                  <FormLabel>{t("popup.graph.destLocation")}</FormLabel>
                  <FormControl>
                    <Select
                      onValueChange={(val) => {
                        if (!val) return;
                        const previousMode = mode;
                        const previousDestExtension = field.value.extension;
                        // Set mode to 'dest' if not determined yet
                        if (mode === null && !src_node && !dest_node) {
                          setMode("dest");
                        }
                        field.onChange({
                          app: app_uri ?? undefined,
                          extension: val,
                        });
                        // Only clear msg_names if:
                        // 1. Switching to dest mode (from null), OR
                        // 2. In dest mode and changing dest extension
                        // Note: In src mode, selecting/changing dest should NOT
                        // clear msg_names because they are based on src
                        const isSwitchingToDestMode =
                          previousMode === null && mode === "dest";
                        if (
                          isSwitchingToDestMode ||
                          (previousMode === "dest" &&
                            previousDestExtension !== val)
                        ) {
                          form.setValue("msg_names", []);
                          form.trigger("msg_names");
                        }
                      }}
                      value={field.value.extension || undefined}
                      disabled={
                        dest_node
                          ? true
                          : mode === null
                            ? false
                            : !!(mode === "src" && !(msgType && primaryMsgName))
                      }
                    >
                      <SelectTrigger
                        className={cn(
                          "w-full overflow-hidden",
                          "[&_.badge]:hidden"
                        )}
                      >
                        <SelectValue
                          placeholder={t("popup.graph.destLocation")}
                        >
                          {field.value.extension}
                          {field.value.extension && " (extension)"}
                        </SelectValue>
                      </SelectTrigger>
                      <SelectContent>
                        <SelectGroup>
                          <SelectLabel>
                            {t("popup.graph.destLocation")}
                          </SelectLabel>
                          {destNodes
                            .filter(
                              (n) =>
                                n.id !== src_node?.id &&
                                n.data.name !== currentSrcExtension
                            )
                            .sort((a, b) => {
                              const aCompatible =
                                compatibleMessagesExtList.includes(
                                  a.data.addon as string
                                );
                              const bCompatible =
                                compatibleMessagesExtList.includes(
                                  b.data.addon as string
                                );
                              return aCompatible === bCompatible
                                ? 0
                                : aCompatible
                                  ? -1
                                  : 1;
                            })
                            .map((node) => (
                              <SelectItem
                                key={node.id}
                                value={node.data.name as string}
                              >
                                {node.data.name as string}{" "}
                                {compatibleMessagesExtList.includes(
                                  node.data.addon as string
                                ) && (
                                  <Badge
                                    className={cn(
                                      "badge",
                                      "bg-ten-green-6 hover:bg-ten-green-6"
                                    )}
                                  >
                                    {t("extensionStore.compatible")}
                                  </Badge>
                                )}
                              </SelectItem>
                            ))}
                        </SelectGroup>
                      </SelectContent>
                    </Select>
                  </FormControl>
                  <FormMessage />
                </FormItem>
              )}
            />
            {(!!dest_node || (mode === "dest" && currentDestExtension)) && (
              <MessageTypeAndNameFields />
            )}
          </div>
        </div>
        <div className="flex w-full">
          <Button type="submit" disabled={isSubmitting} className="ml-auto">
            {isSubmitting && <SpinnerLoading className="size-4" />}
            {t("popup.graph.addConnection")}
          </Button>
        </div>
      </form>
    </Form>
  );
};

// Selector to Extension Connection Widget
const SelectorToExtConnectionWidget = (props: {
  base_dir?: string | null;
  app_uri?: string | null;
  graph_id: string;
  src_node?: TCustomNode | null;
  dest_node?: TCustomNode | null;
  postAddConnectionActions?: () => void | Promise<void>;
}) => {
  const { app_uri, graph_id, src_node, dest_node, postAddConnectionActions } =
    props;
  const [isSubmitting, setIsSubmitting] = React.useState(false);
  // Store selected message keys (format: "msgType:msgName")
  const [selectedMessageKeys, setSelectedMessageKeys] = React.useState<
    string[]
  >([]);

  const { t } = useTranslation();
  const { nodes } = useFlowStore();
  const {
    data: graphs,
    isLoading: isGraphsLoading,
    error: graphError,
    mutate: mutateGraphs,
  } = useGraphs();

  // Get current graph data
  const currentGraph = React.useMemo(
    () => graphs?.find((g) => g.graph_id === graph_id),
    [graphs, graph_id]
  );

  // Extract selector messages with direction "in"
  const selectorInMessages = React.useMemo(() => {
    if (!src_node || src_node.type !== ECustomNodeType.SELECTOR) {
      return [];
    }
    const messages =
      (src_node.data.messages as
        | Array<{
            msg_type: EConnectionType;
            msg_name: string;
            direction: "in" | "out";
            node_name: string;
          }>
        | null
        | undefined) || [];
    return messages.filter((msg) => msg.direction === "in");
  }, [src_node]);

  // Group messages by msg_type and msg_name, collecting all source nodes
  const messagesByTypeAndName = React.useMemo(() => {
    const grouped: Record<EConnectionType, Record<string, string[]>> = {
      [EConnectionType.CMD]: {},
      [EConnectionType.DATA]: {},
      [EConnectionType.AUDIO_FRAME]: {},
      [EConnectionType.VIDEO_FRAME]: {},
    };

    selectorInMessages.forEach((msg) => {
      const msgType = msg.msg_type;
      const msgName = msg.msg_name;
      const srcNodeName = msg.node_name;

      if (!grouped[msgType][msgName]) {
        grouped[msgType][msgName] = [];
      }
      if (!grouped[msgType][msgName].includes(srcNodeName)) {
        grouped[msgType][msgName].push(srcNodeName);
      }
    });

    return grouped;
  }, [selectorInMessages]);

  const form = useForm<z.infer<typeof AddConnectionPayloadSchema>>({
    resolver: zodResolver(AddConnectionPayloadSchema),
    defaultValues: {
      graph_id: graph_id ?? "",
      src: {
        app: app_uri,
        selector:
          src_node?.type === ECustomNodeType.SELECTOR &&
          typeof src_node?.data.name === "string"
            ? src_node.data.name
            : undefined,
      },
      dest: {
        app: app_uri,
        extension:
          dest_node?.type === ECustomNodeType.EXTENSION &&
          typeof dest_node?.data.name === "string"
            ? dest_node.data.name
            : undefined,
      },
      msg_names: [],
      msg_type: EConnectionType.CMD,
    },
  });

  const currentDestExtension = form.watch("dest.extension");
  const selectedMsgType = form.watch("msg_type");

  // Get existing connections from selector to dest extension
  const existingConnections = React.useMemo(() => {
    if (!currentGraph || !src_node) return new Set<string>();

    const selectorName =
      typeof src_node.data.name === "string" ? src_node.data.name : "";
    const destExtensionName =
      dest_node && typeof dest_node.data.name === "string"
        ? dest_node.data.name
        : currentDestExtension;
    if (!destExtensionName) return new Set<string>();

    const existing = new Set<string>();

    currentGraph.graph.connections?.forEach((conn) => {
      // Check if connection is from selector to dest extension
      // For selector -> extension connections, the structure is:
      // { selector: "...", [msgType]: [{ name: "...", dest: [...] }] }
      if (conn.selector === selectorName) {
        // Check all message types
        Object.values(EConnectionType).forEach((msgType) => {
          const flows = conn[msgType];
          if (flows) {
            flows.forEach((flow) => {
              // Check if this flow has dest pointing to our target extension
              const hasDestExtension =
                flow.dest?.some(
                  (dest) => dest.extension === destExtensionName
                ) ?? false;

              if (hasDestExtension) {
                if (flow.name) {
                  existing.add(`${msgType}:${flow.name}`);
                }
                if (flow.names) {
                  flow.names.forEach((name) => {
                    existing.add(`${msgType}:${name}`);
                  });
                }
              }
            });
          }
        });
      }
    });

    return existing;
  }, [currentGraph, src_node, dest_node, currentDestExtension]);

  // Build options list for selected msg_type, showing source nodes
  const messageOptions = React.useMemo(() => {
    if (!selectedMsgType) return [];

    const options: Array<{
      value: string;
      label: string;
      selectable?: boolean;
    }> = [];

    const messagesForType = messagesByTypeAndName[selectedMsgType] || {};
    Object.entries(messagesForType).forEach(([msgName, sourceNodes]) => {
      const key = `${selectedMsgType}:${msgName}`;
      const isExisting = existingConnections.has(key);
      const sourceNodesLabel =
        sourceNodes.length > 0
          ? // eslint-disable-next-line max-len
            ` (${t("popup.graph.messagesFrom", { source: sourceNodes.join(", ") })})`
          : "";
      options.push({
        value: key,
        label: `${msgName}${sourceNodesLabel}`,
        // Disable if connection already exists
        selectable: !isExisting,
      });
    });

    return options;
  }, [messagesByTypeAndName, selectedMsgType, existingConnections, t]);

  // Get default selected values (existing connections)
  const defaultSelectedKeys = React.useMemo(
    () => Array.from(existingConnections),
    [existingConnections]
  );

  // Initialize selectedMessageKeys with existing connections
  // when dest and msg_type are selected
  React.useEffect(() => {
    if (currentDestExtension && selectedMsgType) {
      // Filter to only include keys matching selected msg_type
      const filteredKeys = defaultSelectedKeys.filter((key) => {
        const [msgType] = key.split(":");
        return msgType === selectedMsgType;
      });
      setSelectedMessageKeys(filteredKeys);
      // Also update form with msg_names for validation
      form.setValue(
        "msg_names",
        filteredKeys.map((key) => {
          const [, name] = key.split(":");
          return name;
        })
      );
    } else {
      // Reset if dest/msg_type is not selected
      setSelectedMessageKeys([]);
      form.setValue("msg_names", []);
    }
  }, [currentDestExtension, selectedMsgType, defaultSelectedKeys, form]);

  const onSubmit = async (data: z.infer<typeof AddConnectionPayloadSchema>) => {
    setIsSubmitting(true);
    try {
      // Filter out existing connections - only submit newly selected ones
      const newMessageKeys = selectedMessageKeys.filter(
        (key) => !existingConnections.has(key)
      );

      // Extract msg_names from new keys (all should be same msg_type)
      const msgNames = newMessageKeys
        .map((key) => {
          const [, name] = key.split(":");
          return name;
        })
        .filter((name): name is string => !!name);

      if (msgNames.length === 0) {
        throw new Error("Please select at least one new message name");
      }

      const payload = AddConnectionPayloadSchema.parse({
        ...data,
        msg_type: selectedMsgType,
        msg_names: msgNames,
      });
      await postAddConnection(payload);
      await mutateGraphs();
      postAddConnectionActions?.();
    } catch (error) {
      toast.error(error instanceof Error ? error.message : "Unknown error");
      console.error(error);
    } finally {
      setIsSubmitting(false);
    }
  };

  // Get available dest nodes (extension nodes only)
  const destNodes = React.useMemo(() => {
    return nodes.filter(
      (n) =>
        n.data.graph.graph_id === graph_id &&
        n.type === ECustomNodeType.EXTENSION
    );
  }, [nodes, graph_id]);

  // biome-ignore lint/correctness/useExhaustiveDependencies: <ignore>
  React.useEffect(() => {
    if (graphError) {
      toast.error(t("popup.graph.graphError"), {
        description: graphError.message,
      });
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [graphError]);

  return (
    <Form {...form}>
      <form
        onSubmit={form.handleSubmit(onSubmit)}
        className="h-full w-full space-y-4 overflow-y-auto px-2"
      >
        <FormField
          control={form.control}
          name="graph_id"
          render={({ field }) => (
            <FormItem>
              <FormLabel>{t("popup.graph.graphName")}</FormLabel>
              <FormControl>
                <Select
                  onValueChange={field.onChange}
                  defaultValue={field.value}
                  disabled={!!graph_id}
                >
                  <SelectTrigger className="w-full" disabled={isGraphsLoading}>
                    <SelectValue placeholder={t("popup.graph.graphName")} />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectGroup>
                      <SelectLabel>{t("popup.graph.graphName")}</SelectLabel>
                      {isGraphsLoading ? (
                        <SelectItem value={t("popup.graph.graphName")}>
                          <SpinnerLoading className="size-4" />
                        </SelectItem>
                      ) : (
                        graphs?.map((graph) => (
                          <SelectItem
                            key={graph.graph_id}
                            value={graph.graph_id}
                          >
                            {graph.name}
                          </SelectItem>
                        ))
                      )}
                    </SelectGroup>
                  </SelectContent>
                </Select>
              </FormControl>
              <FormMessage />
            </FormItem>
          )}
        />
        <div className="flex items-center justify-between gap-4">
          <div className="flex-1 space-y-4 rounded-md bg-muted/50 p-4">
            <FormItem>
              <FormLabel>{t("popup.graph.srcLocation")}</FormLabel>
              <div className="text-muted-foreground text-sm">
                {typeof src_node?.data.name === "string"
                  ? `${src_node.data.name} (selector)`
                  : "selector"}
              </div>
            </FormItem>
          </div>
          <ArrowBigRightIcon className="mx-auto size-4" />
          <div className="flex-1 space-y-4 rounded-md bg-muted/50 p-4">
            <FormField
              control={form.control}
              name="dest"
              render={({ field }) => (
                <FormItem>
                  <FormLabel>{t("popup.graph.destLocation")}</FormLabel>
                  <FormControl>
                    <Select
                      onValueChange={(val) => {
                        if (!val) return;
                        field.onChange({
                          app: app_uri ?? undefined,
                          extension: val,
                        });
                        // Reset messages and msg_type when dest changes
                        setSelectedMessageKeys([]);
                        form.setValue("msg_names", []);
                        form.setValue("msg_type", EConnectionType.CMD);
                        form.trigger("msg_names");
                      }}
                      value={field.value.extension || undefined}
                      disabled={!!dest_node}
                    >
                      <SelectTrigger className="w-full">
                        <SelectValue
                          placeholder={t("popup.graph.destLocation")}
                        >
                          {field.value.extension}
                          {field.value.extension && " (extension)"}
                        </SelectValue>
                      </SelectTrigger>
                      <SelectContent>
                        <SelectGroup>
                          <SelectLabel>
                            {t("popup.graph.destLocation")}
                          </SelectLabel>
                          {destNodes
                            .filter((n) => n.id !== src_node?.id)
                            .map((node) => (
                              <SelectItem
                                key={node.id}
                                value={node.data.name as string}
                              >
                                {node.data.name as string}
                              </SelectItem>
                            ))}
                        </SelectGroup>
                      </SelectContent>
                    </Select>
                  </FormControl>
                  <FormMessage />
                </FormItem>
              )}
            />
            {form.watch("dest.extension") && (
              <>
                <FormField
                  control={form.control}
                  name="msg_type"
                  render={({ field }) => (
                    <FormItem>
                      <FormLabel>{t("popup.graph.messageType")}</FormLabel>
                      <FormControl>
                        <Select
                          onValueChange={(val) => {
                            field.onChange(val);
                            // Reset selected messages when msg_type changes
                            setSelectedMessageKeys([]);
                            form.setValue("msg_names", []);
                            form.trigger("msg_names");
                          }}
                          value={field.value}
                        >
                          <SelectTrigger className="w-full overflow-hidden">
                            <SelectValue
                              placeholder={t("popup.graph.messageType")}
                            />
                          </SelectTrigger>
                          <SelectContent>
                            <SelectGroup>
                              <SelectLabel>
                                {t("popup.graph.messageType")}
                              </SelectLabel>
                              {Object.values(EConnectionType).map((type) => (
                                <SelectItem key={type} value={type}>
                                  {type}
                                </SelectItem>
                              ))}
                            </SelectGroup>
                          </SelectContent>
                        </Select>
                      </FormControl>
                      <FormMessage />
                    </FormItem>
                  )}
                />
                {selectedMsgType && (
                  <FormItem>
                    <FormLabel>{t("popup.graph.messageName")}</FormLabel>
                    <FormControl>
                      <MultiSelectorWithCheckbox
                        options={messageOptions}
                        placeholder={t("popup.graph.messageName")}
                        selected={selectedMessageKeys}
                        onChange={(items) => {
                          // Store full keys (msgType:msgName format)
                          const keys = items.map((item) => item.value);
                          setSelectedMessageKeys(keys);
                          // Also update form with just names for validation
                          form.setValue(
                            "msg_names",
                            keys.map((key) => {
                              const [, name] = key.split(":");
                              return name;
                            })
                          );
                        }}
                        selectAllLabel={t("action.selectAll")}
                      />
                    </FormControl>
                    <FormMessage />
                  </FormItem>
                )}
              </>
            )}
          </div>
        </div>
        <div className="flex w-full">
          <Button type="submit" disabled={isSubmitting} className="ml-auto">
            {isSubmitting && <SpinnerLoading className="size-4" />}
            {t("popup.graph.addConnection")}
          </Button>
        </div>
      </form>
    </Form>
  );
};
