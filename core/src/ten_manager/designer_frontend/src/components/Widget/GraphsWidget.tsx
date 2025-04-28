//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as React from "react";
import { z } from "zod";
import { zodResolver } from "@hookform/resolvers/zod";
import { useForm } from "react-hook-form";
import { useTranslation } from "react-i18next";
import { toast } from "sonner";
import { ZodProvider } from "@autoform/zod";
import { EditIcon } from "lucide-react";

import {
  AddNodePayloadSchema,
  AddConnectionPayloadSchema,
  EConnectionType,
  UpdateNodePropertyPayloadSchema,
  IGraph,
} from "@/types/graphs";
import { Button } from "@/components/ui/Button";
import {
  Form,
  FormControl,
  //   FormDescription,
  FormField,
  FormItem,
  FormLabel,
  FormMessage,
} from "@/components/ui/Form";
import {
  Select,
  SelectContent,
  SelectGroup,
  SelectItem,
  SelectLabel,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/Select";
import { Input } from "@/components/ui/Input";
import { SpinnerLoading } from "@/components/Status/Loading";
import { Combobox } from "@/components/ui/Combobox";
import {
  useGraphs,
  postAddNode,
  retrieveGraphNodes,
  retrieveGraphConnections,
  postAddConnection,
  postUpdateNodeProperty,
  postReplaceNode,
} from "@/api/services/graphs";
import {
  retrieveExtensionSchema,
  retrieveExtensionDefaultProperty,
} from "@/api/services/extension";
import { useAddons } from "@/api/services/addons";
import {
  generateRawNodes,
  generateRawEdges,
  updateNodesWithConnections,
  updateNodesWithAddonInfo,
  generateNodesAndEdges,
  syncGraphNodeGeometry,
} from "@/flow/graph";
import { useAppStore, useDialogStore, useFlowStore } from "@/store";
import type { TCustomNode } from "@/types/flow";
import { AutoForm } from "@/components/ui/autoform";
// eslint-disable-next-line max-len
import { convertExtensionPropertySchema2ZodSchema } from "@/components/Widget/utils";
import { cn } from "@/lib/utils";

// eslint-disable-next-line react-refresh/only-export-components
export const resetNodesAndEdgesByGraph = async (graph: IGraph) => {
  const backendNodes = await retrieveGraphNodes(graph.uuid);
  const backendConnections = await retrieveGraphConnections(graph.uuid);
  const rawNodes = generateRawNodes(backendNodes);
  const [rawEdges, rawEdgeAddressMap] = generateRawEdges(backendConnections);
  const nodesWithConnections = updateNodesWithConnections(
    rawNodes,
    rawEdgeAddressMap
  );
  const nodesWithAddonInfo = await updateNodesWithAddonInfo(
    graph.base_dir,
    nodesWithConnections
  );

  const { nodes: layoutedNodes, edges: layoutedEdges } = generateNodesAndEdges(
    nodesWithAddonInfo,
    rawEdges
  );

  const nodesWithGeometry = await syncGraphNodeGeometry(
    graph.uuid,
    layoutedNodes
  );

  return { nodes: nodesWithGeometry, edges: layoutedEdges };
};

const GraphAddNodePropertyField = (props: {
  addon: string;
  onChange?: (value: Record<string, unknown> | undefined) => void;
}) => {
  const { addon, onChange } = props;

  const [isLoading, setIsLoading] = React.useState(false);
  const [errMsg, setErrMsg] = React.useState<string | null>(null);
  const [propertySchemaEntries, setPropertySchemaEntries] = React.useState<
    [string, z.ZodType][]
  >([]);
  const [defaultProperty, setDefaultProperty] = React.useState<
    Record<string, unknown> | undefined | null
  >(null);

  const { t } = useTranslation();
  const { currentWorkspace } = useAppStore();
  const { appendDialog, removeDialog } = useDialogStore();

  const isSchemaEmptyMemo = React.useMemo(() => {
    return !isLoading && propertySchemaEntries.length === 0;
  }, [isLoading, propertySchemaEntries.length]);

  React.useEffect(() => {
    const init = async () => {
      try {
        setIsLoading(true);

        const addonSchema = await retrieveExtensionSchema({
          appBaseDir: currentWorkspace?.app?.base_dir ?? "",
          addonName: addon,
        });
        const propertySchema = addonSchema.property;
        if (!propertySchema) {
          // toast.error(t("popup.graph.noPropertySchema"));
          return;
        }
        const defaultProperty = await retrieveExtensionDefaultProperty({
          appBaseDir: currentWorkspace?.app?.base_dir ?? "",
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
            <div className="flex flex-row gap-2 w-full justify-end">
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
    <div className="flex flex-col gap-2 w-full h-fit">
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
  base_dir: string;
  graph_id?: string;
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
  const { currentWorkspace } = useAppStore();
  const { setNodesAndEdges } = useFlowStore();

  const form = useForm<z.infer<typeof AddNodePayloadSchema>>({
    resolver: zodResolver(AddNodePayloadSchema),
    defaultValues: {
      graph_id: graph_id ?? currentWorkspace?.graph?.uuid ?? "",
      name: node?.data?.name || undefined,
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
      if (
        currentWorkspace?.graph?.uuid &&
        (currentWorkspace?.graph?.uuid === data.graph_id || isReplaceNode)
      ) {
        const { nodes, edges } = await resetNodesAndEdgesByGraph(
          currentWorkspace.graph
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

  const { graphs, isLoading: isGraphsLoading, error: graphError } = useGraphs();
  const {
    addons,
    isLoading: isAddonsLoading,
    error: addonError,
  } = useAddons(base_dir);

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
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [graphError, addonError]);

  return (
    <Form {...form}>
      <form
        onSubmit={form.handleSubmit(onSubmit)}
        className="space-y-4 w-full h-full overflow-y-auto px-2"
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
                  />
                </FormControl>
                <FormMessage />
              </FormItem>
            )}
          />
        )}

        {remoteCheckErrorMessage && (
          <div className="text-red-500 flex flex-col gap-2">
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

export const GraphAddConnectionWidget = (props: {
  base_dir: string;
  app_uri?: string | null;
  graph_id?: string;
  src_extension?: string;
  dest_extension?: string;
  postAddConnectionActions?: () => void | Promise<void>;
}) => {
  const {
    base_dir,
    app_uri,
    graph_id,
    src_extension,
    dest_extension,
    postAddConnectionActions,
  } = props;
  const [isSubmitting, setIsSubmitting] = React.useState(false);
  const [isMsgNameLoading, setIsMsgNameLoading] = React.useState(false);
  const [msgNameError, setMsgNameError] = React.useState<unknown | null>(null);
  const [msgNameList, setMsgNameList] = React.useState<
    {
      value: string;
      label: string;
    }[]
  >([]);

  const { t } = useTranslation();
  const { nodes, setNodesAndEdges } = useFlowStore();
  const { graphs, isLoading: isGraphsLoading, error: graphError } = useGraphs();
  const { currentWorkspace } = useAppStore();

  const form = useForm<z.infer<typeof AddConnectionPayloadSchema>>({
    resolver: zodResolver(AddConnectionPayloadSchema),
    defaultValues: {
      graph_id: graph_id ?? currentWorkspace?.graph?.uuid ?? "",
      src_app: app_uri,
      src_extension: src_extension ?? undefined,
      msg_type: EConnectionType.CMD,
      msg_name: undefined,
      dest_app: app_uri,
      dest_extension: dest_extension ?? undefined,
    },
  });

  const srcExtension = form.watch("src_extension");
  const destExtension = form.watch("dest_extension");
  const msgType = form.watch("msg_type");

  const onSubmit = async (data: z.infer<typeof AddConnectionPayloadSchema>) => {
    setIsSubmitting(true);
    try {
      const payload = AddConnectionPayloadSchema.parse(data);
      if (payload.src_extension === payload.dest_extension) {
        throw new Error(t("popup.graph.sameNodeError"));
      }
      await postAddConnection(payload);
      if (
        currentWorkspace?.graph?.uuid === data.graph_id &&
        currentWorkspace?.app?.base_dir === base_dir
      ) {
        const { nodes, edges } = await resetNodesAndEdgesByGraph(
          currentWorkspace.graph
        );
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

  React.useEffect(() => {
    if (graphError) {
      toast.error(t("popup.graph.graphError"), {
        description: graphError.message,
      });
    }
    if (msgNameError) {
      toast.error(t("popup.graph.addonError"), {
        description:
          msgNameError instanceof Error
            ? msgNameError.message
            : "Unknown error",
      });
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [graphError, msgNameError]);

  React.useEffect(() => {
    const fetchNodeSchema = async () => {
      const sourceNode = nodes.find((i) => i.data.name === srcExtension);
      const destNode = nodes.find((i) => i.data.name === destExtension);
      if (!sourceNode && !destNode) return;
      try {
        form.setValue("msg_name", undefined as unknown as string);
        setIsMsgNameLoading(true);
        let msgNameList: { value: string; label: string }[] = [];

        if (sourceNode) {
          const nodeSchema = await retrieveExtensionSchema({
            appBaseDir: currentWorkspace?.app?.base_dir ?? "",
            addonName: sourceNode.data.addon,
          });
          const srcMsgNameList =
            nodeSchema?.[`${msgType}_out`]?.map((i) => i.name) ?? [];
          msgNameList = [
            ...msgNameList,
            ...srcMsgNameList.map((i) => ({
              value: i,
              label: `${i} (${sourceNode.data.name})`,
            })),
          ];
        }

        if (destNode) {
          const nodeSchema = await retrieveExtensionSchema({
            appBaseDir: currentWorkspace?.app?.base_dir ?? "",
            addonName: destNode.data.addon,
          });
          const destMsgNameList =
            nodeSchema?.[`${msgType}_in`]?.map((i) => i.name) ?? [];
          msgNameList = [
            ...msgNameList,
            ...destMsgNameList.map((i) => ({
              value: i,
              label: `${i} (${destNode.data.name})`,
            })),
          ];
        }

        setMsgNameList(msgNameList);
      } catch (error: unknown) {
        console.error(error);
        setMsgNameError(error);
      } finally {
        setIsMsgNameLoading(false);
      }
    };

    fetchNodeSchema();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [srcExtension, destExtension, msgType, nodes]);

  console.log("isMsgNameLoading ===", isMsgNameLoading);

  return (
    <Form {...form}>
      <form
        onSubmit={form.handleSubmit(onSubmit)}
        className="space-y-4 w-full h-full overflow-y-auto px-2"
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
                  disabled={!!src_extension}
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
                  disabled={!!src_extension}
                >
                  <SelectTrigger className="w-full overflow-hidden">
                    <SelectValue placeholder={t("popup.graph.srcExtension")} />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectGroup>
                      <SelectLabel>{t("popup.graph.srcExtension")}</SelectLabel>
                      {nodes.map((node) => (
                        <SelectItem key={node.id} value={node.data.name}>
                          {node.data.name}
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
        <FormField
          control={form.control}
          name="msg_type"
          render={({ field }) => (
            <FormItem>
              <FormLabel>{t("popup.graph.messageType")}</FormLabel>
              <FormControl>
                <Select onValueChange={field.onChange} value={field.value}>
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
        {form.watch("msg_type") &&
          (form.watch("src_extension") || form.watch("dest_extension")) && (
            <FormField
              control={form.control}
              name="msg_name"
              render={({ field }) => (
                <FormItem>
                  <FormLabel>{t("popup.graph.messageName")}</FormLabel>
                  <FormControl>
                    <Combobox
                      key={`${msgType}` + "-" + `${srcExtension}`}
                      disabled={isMsgNameLoading}
                      isLoading={isMsgNameLoading}
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
                >
                  <SelectTrigger className="w-full overflow-hidden">
                    <SelectValue placeholder={t("popup.graph.destExtension")} />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectGroup>
                      <SelectLabel>
                        {t("popup.graph.destExtension")}
                      </SelectLabel>
                      {nodes
                        .filter((i) => i.id !== srcExtension)
                        .map((node) => (
                          <SelectItem key={node.id} value={node.data.name}>
                            {node.data.name}
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
        <Button type="submit" disabled={isSubmitting}>
          {isSubmitting && <SpinnerLoading className="size-4" />}
          {t("popup.graph.addConnection")}
        </Button>
      </form>
    </Form>
  );
};

export const GraphUpdateNodePropertyWidget = (props: {
  base_dir: string;
  app_uri?: string | null;
  graph_id?: string;
  node: TCustomNode;
  postUpdateNodePropertyActions?: () => void | Promise<void>;
}) => {
  const { app_uri, graph_id, node, postUpdateNodePropertyActions } = props;
  const [isSubmitting, setIsSubmitting] = React.useState(false);
  const [isSchemaLoading, setIsSchemaLoading] = React.useState(false);
  const [propertySchemaEntries, setPropertySchemaEntries] = React.useState<
    [string, z.ZodType][]
  >([]);

  const { t } = useTranslation();

  const { setNodesAndEdges } = useFlowStore();
  const { currentWorkspace } = useAppStore();

  React.useEffect(() => {
    const fetchSchema = async () => {
      try {
        setIsSchemaLoading(true);
        const schema = await retrieveExtensionSchema({
          appBaseDir: currentWorkspace?.app?.base_dir ?? "",
          addonName: node.data.addon,
        });
        const propertySchemaEntries = convertExtensionPropertySchema2ZodSchema(
          schema.property ?? {}
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
  }, [currentWorkspace?.app?.base_dir, node.data.addon]);

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
                graph_id: graph_id ?? currentWorkspace?.graph?.uuid ?? "",
                name: node.data.name,
                addon: node.data.addon,
                extension_group: node.data.extension_group,
                app: app_uri ?? undefined,
                property: JSON.stringify(data, null, 2),
              });
              await postUpdateNodeProperty(nodeData);
              if (currentWorkspace?.graph) {
                const { nodes, edges } = await resetNodesAndEdgesByGraph(
                  currentWorkspace.graph
                );
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
              "flex flex-col gap-4 overflow-y-auto px-1 h-full",
              isSubmitting && "disabled"
            ),
          }}
        />
      ) : (
        <div className="text-center text-sm text-gray-500">
          {t("popup.graph.noPropertySchema")}
        </div>
      )}
    </>
  );
};
