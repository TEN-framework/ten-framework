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
import { AutoForm } from "@/components/ui/autoform";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { Combobox } from "@/components/ui/combobox";
import {
  Form,
  FormControl,
  //   FormDescription,
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
// eslint-disable-next-line max-len
import { convertExtensionPropertySchema2ZodSchema } from "@/components/widget/utils";
import { resetNodesAndEdgesByGraphs } from "@/flow/graph";
import { cn } from "@/lib/utils";
import { useDialogStore, useFlowStore } from "@/store";
import type { TCustomNode } from "@/types/flow";
import {
  AddConnectionPayloadSchema,
  AddNodePayloadSchema,
  EConnectionType,
  EMsgDirection,
  UpdateNodePropertyPayloadSchema,
} from "@/types/graphs";

const GraphAddNodePropertyField = (props: {
  base_dir?: string;
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
          <AutoForm
            onSubmit={async (data) => {
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
          </AutoForm>
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
  base_dir?: string;
  graph_id: string;
  postAddNodeActions?: () => void | Promise<void>;
  node?: TCustomNode;
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
  const { setNodesAndEdges } = useFlowStore();

  const {
    data: graphs,
    isLoading: isGraphsLoading,
    error: graphError,
  } = useGraphs();
  const {
    data: addons,
    isLoading: isAddonsLoading,
    error: addonError,
  } = useFetchAddons({ base_dir });

  const form = useForm<z.infer<typeof AddNodePayloadSchema>>({
    resolver: zodResolver(AddNodePayloadSchema),
    defaultValues: {
      graph_id: graph_id ?? node?.data?.graph?.uuid ?? "",
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
      if (graph_id === data.graph_id || isReplaceNode) {
        const { nodes, edges } = await resetNodesAndEdgesByGraphs(
          graphs?.filter((g) => g.uuid === data.graph_id) || []
        );
        setNodesAndEdges(nodes, edges);
        postAddNodeActions?.();
      }
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
                          <SelectItem key={graph.uuid} value={graph.uuid}>
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
  base_dir?: string;
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

  const { t } = useTranslation();

  const { setNodesAndEdges } = useFlowStore();

  const { data: graphs } = useGraphs();

  React.useEffect(() => {
    const fetchSchema = async () => {
      try {
        setIsSchemaLoading(true);
        const schema = await retrieveExtensionSchema({
          appBaseDir: base_dir ?? "",
          addonName: typeof node.data.addon === "string" ? node.data.addon : "",
        });
        const propertySchemaEntries = convertExtensionPropertySchema2ZodSchema(
          schema.property?.properties ?? {}
        );
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
    <>
      {isSchemaLoading && !propertySchemaEntries && (
        <SpinnerLoading className="size-4" />
      )}
      {propertySchemaEntries?.length > 0 ? (
        <AutoForm
          values={node?.data.property || {}}
          schema={
            new ZodProvider(z.object(Object.fromEntries(propertySchemaEntries)))
          }
          onSubmit={async (data) => {
            setIsSubmitting(true);
            try {
              const nodeData = UpdateNodePropertyPayloadSchema.parse({
                graph_id: graph_id ?? node?.data?.graph?.uuid ?? "",
                name: node.data.name,
                addon: node.data.addon,
                extension_group: node.data.extension_group,
                app: app_uri ?? undefined,
                property: JSON.stringify(data, null, 2),
              });
              await postUpdateNodeProperty(nodeData);
              const targetGraph = graphs?.find(
                (g) => g.uuid === nodeData.graph_id
              );
              if (targetGraph) {
                const { nodes, edges } = await resetNodesAndEdgesByGraphs([
                  targetGraph,
                ]);
                setNodesAndEdges(nodes, edges);
              }
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
    </>
  );
};

export const GraphConnectionCreationWidget = (props: {
  base_dir?: string;
  app_uri?: string | null;
  graph_id: string;
  src_node?: TCustomNode;
  dest_node?: TCustomNode;
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
    }[]
  >([]);

  const { t } = useTranslation();
  const { nodes, setNodesAndEdges } = useFlowStore();
  const {
    data: graphs,
    isLoading: isGraphsLoading,
    error: graphError,
  } = useGraphs();
  const {
    data: extSchema,
    isLoading: isExtSchemaLoading,
    error: extSchemaError,
  } = useFetchExtSchema(
    src_node || dest_node
      ? {
          appBaseDir: base_dir ?? "",
          addonName: (src_node?.data.addon || dest_node?.data.addon) as string,
        }
      : null
  );

  const form = useForm<z.infer<typeof AddConnectionPayloadSchema>>({
    resolver: zodResolver(AddConnectionPayloadSchema),
    defaultValues: {
      graph_id: graph_id ?? "",
      src_app: app_uri,
      src_extension:
        typeof src_node?.data.name === "string"
          ? src_node.data.name
          : undefined,
      msg_type: EConnectionType.CMD,
      msg_name: undefined,
      dest_app: app_uri,
      dest_extension:
        typeof dest_node?.data.name === "string"
          ? dest_node.data.name
          : undefined,
    },
  });

  const onSubmit = async (data: z.infer<typeof AddConnectionPayloadSchema>) => {
    setIsSubmitting(true);
    try {
      const payload = AddConnectionPayloadSchema.parse(data);
      if (payload.src_extension === payload.dest_extension) {
        throw new Error(t("popup.graph.sameNodeError"));
      }
      await postAddConnection(payload);
      const targetGraph = graphs?.find((g) => g.uuid === data.graph_id);
      if (graph_id === data.graph_id && targetGraph) {
        const { nodes, edges } = await resetNodesAndEdgesByGraphs([
          targetGraph,
        ]);
        setNodesAndEdges(nodes, edges);
        postAddConnectionActions?.();
      }
    } catch (error) {
      toast.error(error instanceof Error ? error.message : "Unknown error");
      console.error(error);
    } finally {
      setIsSubmitting(false);
    }
  };

  const {
    data: compatibleMessages,
    isLoading: isCompatibleMsgLoading,
    error: compatibleMsgError,
  } = useCompatibleMessages(
    (src_node || dest_node) &&
      form.watch("msg_type") &&
      form.watch("msg_name") &&
      graph_id &&
      !(src_node && dest_node)
      ? {
          graph_id: graph_id ?? "",
          app: app_uri ?? undefined,
          extension_group: (src_node?.data.extension_group ||
            dest_node?.data.extension_group) as string | undefined,
          extension: (src_node?.data.name ||
            dest_node?.data.name ||
            "") as string,
          msg_type: form.watch("msg_type"),
          msg_direction: src_node?.data.name
            ? EMsgDirection.OUT
            : EMsgDirection.IN,
          msg_name: form.watch("msg_name"),
        }
      : null
  );

  const compatibleMessagesExtList = React.useMemo(() => {
    if (!compatibleMessages) return [];
    return compatibleMessages.map((i) => i.extension);
  }, [compatibleMessages]);

  const [srcNodes, destNodes] = React.useMemo(() => {
    return nodes.reduce(
      (prev, cur) => {
        if (cur.data.name === src_node?.data.name) {
          prev[0].push(cur);
          return prev;
        }
        if (cur.data.name === dest_node?.data.name) {
          prev[1].push(cur);
          return prev;
        }
        const targetArray = src_node ? prev[1] : prev[0];
        targetArray.push(cur);
        return prev;
      },
      [[], []] as [TCustomNode[], TCustomNode[]]
    );
  }, [nodes, src_node, dest_node?.data.name]);

  React.useEffect(() => {
    const direction = src_node?.data.name ? "out" : "in";
    if (extSchema) {
      const srcMsgNameList =
        extSchema?.[`${form.watch("msg_type")}_${direction}`]?.map(
          (i) => i.name
        ) ?? [];
      const newMsgNameList = [
        ...srcMsgNameList.map((i) => ({
          value: i,
          label: `${i}`,
        })),
      ];
      setMsgNameList(newMsgNameList);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [extSchema, form.watch("msg_type"), src_node?.data.name]);

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

  const Inner = () => {
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
                    form.setValue("msg_name", undefined as unknown as string);
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
            name="msg_name"
            render={({ field }) => (
              <FormItem>
                <FormLabel>{t("popup.graph.messageName")}</FormLabel>
                <FormControl>
                  <Combobox
                    // eslint-disable-next-line max-len
                    key={`${form.watch("msg_type")}-${form.watch("src_extension")}`}
                    disabled={isExtSchemaLoading}
                    isLoading={isExtSchemaLoading}
                    options={msgNameList}
                    placeholder={t("popup.graph.messageName")}
                    selected={field.value}
                    onChange={(i) => {
                      field.onChange(i.value);
                    }}
                    onCreate={(i) => {
                      setMsgNameList((prev) => [
                        ...prev,
                        {
                          value: i,
                          label: i,
                        },
                      ]);
                      field.onChange(i);
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
                          <SelectItem key={graph.uuid} value={graph.uuid}>
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
              name="src_extension"
              render={({ field }) => (
                <FormItem>
                  <FormLabel>{t("popup.graph.srcExtension")}</FormLabel>
                  <FormControl>
                    <Select
                      onValueChange={field.onChange}
                      value={field.value}
                      disabled={
                        (src_node
                          ? true
                          : !(
                              form.watch("msg_type") && form.watch("msg_name")
                            )) || isCompatibleMsgLoading
                      }
                    >
                      <SelectTrigger
                        className={cn(
                          "w-full overflow-hidden",
                          "[&_.badge]:hidden"
                        )}
                      >
                        <SelectValue
                          placeholder={t("popup.graph.srcExtension")}
                        />
                      </SelectTrigger>
                      <SelectContent>
                        <SelectGroup>
                          <SelectLabel>
                            {t("popup.graph.srcExtension")}
                          </SelectLabel>
                          {srcNodes
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
            {!!src_node && <Inner />}
          </div>
          <ArrowBigRightIcon className="mx-auto size-4" />
          <div className="flex-1 space-y-4 rounded-md bg-muted/50 p-4">
            <FormField
              control={form.control}
              name="dest_extension"
              render={({ field }) => (
                <FormItem>
                  <FormLabel>{t("popup.graph.destExtension")}</FormLabel>
                  <FormControl>
                    <Select
                      onValueChange={field.onChange}
                      value={field.value ?? undefined}
                      disabled={
                        (dest_node
                          ? true
                          : !(
                              form.watch("msg_type") && form.watch("msg_name")
                            )) || isCompatibleMsgLoading
                      }
                    >
                      <SelectTrigger
                        className={cn(
                          "w-full overflow-hidden",
                          "[&_.badge]:hidden"
                        )}
                      >
                        <SelectValue
                          placeholder={t("popup.graph.destExtension")}
                        />
                      </SelectTrigger>
                      <SelectContent>
                        <SelectGroup>
                          <SelectLabel>
                            {t("popup.graph.destExtension")}
                          </SelectLabel>
                          {destNodes
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
            {!!dest_node && <Inner />}
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
