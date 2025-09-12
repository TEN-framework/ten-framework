//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import { useReactFlow } from "@xyflow/react";
import { Maximize2Icon, Minimize2Icon } from "lucide-react";
import * as React from "react";
import { useTranslation } from "react-i18next";
import { toast } from "sonner";
import { postGraphsAutoStart, useGraphs } from "@/api/services/graphs";
import { Button } from "@/components/ui/button";
import {
  Card,
  CardAction,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/ui/card";
import { Checkbox } from "@/components/ui/checkbox";
import { Label } from "@/components/ui/label";
import { Separator } from "@/components/ui/separator";
import { resetNodesAndEdgesByGraphs } from "@/flow/graph";
import { calcAbbreviatedBaseDir, cn } from "@/lib/utils";
import { useAppStore, useFlowStore } from "@/store";
import type { GraphInfo } from "@/types/graphs";

export const GraphSelector = (props: { className?: string }) => {
  const { className } = props;

  const [isExpanded, setIsExpanded] = React.useState(false);

  const { t } = useTranslation();
  const {
    data: graphs,
    isLoading: isGraphLoading,
    error: isGraphError,
  } = useGraphs();
  const { selectedGraphs, setSelectedGraphs } = useAppStore();
  const {
    nodes,
    edges,
    setNodesAndEdges,
    setDisplayedEdges,
    setDisplayedNodes,
  } = useFlowStore();

  // Initially set selectedGraphs to all graphs if not already set
  React.useEffect(() => {
    if (!selectedGraphs && !isGraphLoading && graphs) {
      // default select all
      setSelectedGraphs(graphs);
    }
  }, [graphs, isGraphLoading, selectedGraphs, setSelectedGraphs]);

  // Reset nodes and edges in factory when graphs change
  React.useEffect(() => {
    const processNodesAndEdges = async () => {
      if (!graphs) {
        return;
      }
      const { nodes: layoutedNodes, edges: layoutedEdges } =
        await resetNodesAndEdgesByGraphs(graphs);

      setNodesAndEdges(layoutedNodes, layoutedEdges);
    };

    processNodesAndEdges();
  }, [graphs, setNodesAndEdges]);

  // Reset displayed nodes and edges when selectedGraphs change
  React.useEffect(() => {
    if (!selectedGraphs || selectedGraphs.length === 0) {
      setDisplayedNodes([]);
      setDisplayedEdges([]);
      return;
    }
    const nextDisplayedNodes = nodes.filter((node) =>
      selectedGraphs.some(
        (graph) => graph.graph_id === node.data.graph.graph_id
      )
    );
    const nextDisplayedEdges = edges.filter((edge) =>
      selectedGraphs.some(
        (graph) => graph.graph_id === edge.data?.graph.graph_id
      )
    );
    setDisplayedNodes(nextDisplayedNodes);
    setDisplayedEdges(nextDisplayedEdges);
  }, [nodes, edges, selectedGraphs, setDisplayedEdges, setDisplayedNodes]);

  React.useEffect(() => {
    if (isGraphError) {
      console.error("Error loading graphs:", isGraphError);
      toast.error("Error loading graphs");
    }
  }, [isGraphError]);

  return (
    <Card
      className={cn(
        "absolute top-12 right-2",
        "h-fit max-h-[calc(100%-40px-8px-8px-20px-150px-15px-15px)]",
        "gap-0 p-4",
        "w-3xs transition-all duration-300",
        {
          "w-sm": isExpanded,
        },
        className
      )}
    >
      <CardHeader className="px-0">
        <CardTitle>{t("graph.title")}</CardTitle>
        <CardDescription>
          {isGraphLoading
            ? "Loading..."
            : isGraphError
              ? "Error loading graphs"
              : t("graph.selected-sum-count", {
                  count: selectedGraphs?.length || 0,
                  sum: graphs?.length,
                })}
        </CardDescription>
        <CardAction className="flex justify-end">
          {isExpanded ? (
            <Button
              variant="ghost"
              size="icon"
              onClick={() => setIsExpanded(false)}
            >
              <Minimize2Icon />
            </Button>
          ) : (
            <Button
              variant="ghost"
              size="icon"
              className=""
              onClick={() => setIsExpanded(true)}
            >
              <Maximize2Icon />
            </Button>
          )}
        </CardAction>
      </CardHeader>
      <CardContent
        className={cn(
          "flex h-fit flex-col gap-2 overflow-y-auto px-0",
          "transition-all duration-300",
          "max-h-0 overflow-hidden opacity-0",
          {
            "mt-6 max-h-fit opacity-100": isExpanded,
          }
        )}
      >
        {graphs && <GraphList graphs={graphs} />}
      </CardContent>
    </Card>
  );
};

const GraphList = (props: { graphs: GraphInfo[] }) => {
  const [isLoading, setIsLoading] = React.useState(false);

  const { mutate: mutateGraphs } = useGraphs();
  const { fitView } = useReactFlow();

  const { t } = useTranslation();
  const {
    selectedGraphs,
    setSelectedGraphs,
    appendSelectedGraphs,
    removeSelectedGraphs,
  } = useAppStore();

  const groupedGraphs: { standalone: GraphInfo[]; [x: string]: GraphInfo[] } =
    props.graphs.reduce(
      (acc, graph) => {
        const baseDir = graph.base_dir || "standalone";
        if (!acc[baseDir]) {
          acc[baseDir] = [];
        }
        acc[baseDir].push(graph);
        return acc;
      },
      {} as { standalone: GraphInfo[]; [x: string]: GraphInfo[] }
    );

  return (
    <div className="flex flex-col">
      {Object.entries(groupedGraphs).map(([baseDir, graphs], index) => (
        <React.Fragment key={baseDir}>
          <Separator className={cn("mt-2 mb-2", { "mt-0": index === 0 })} />
          <div className="flex flex-col gap-1">
            <div
              className={cn(
                "font-semibold text-xs",
                "w-full overflow-hidden text-ellipsis whitespace-nowrap",
                "flex items-center justify-between",
                "group relative"
              )}
            >
              <span className="overflow-hidden text-ellipsis whitespace-nowrap">
                {calcAbbreviatedBaseDir(baseDir)}
              </span>
              <div
                className={cn(
                  "flex items-center gap-0.5",
                  "-translate-y-1/2 absolute top-1/2 right-0 transform",
                  "opacity-0 transition-opacity group-hover:opacity-100",
                  "overflow-hidden rounded bg-popover"
                )}
              >
                <Button
                  variant="link"
                  size="xs"
                  className={cn("underline")}
                  onClick={() => {
                    setSelectedGraphs(graphs);
                  }}
                >
                  {t("graph.select-all")}
                </Button>
              </div>
            </div>
            {graphs.map((graph) => (
              <div
                key={graph.graph_id}
                className={cn("flex items-center gap-3", "group relative")}
              >
                <Checkbox
                  id={`graph-selector-${graph.graph_id}`}
                  disabled={isLoading}
                  checked={selectedGraphs?.some(
                    (g) => g.graph_id === graph.graph_id
                  )}
                  onCheckedChange={(checked) => {
                    if (checked) {
                      appendSelectedGraphs([graph]);
                    } else {
                      removeSelectedGraphs([graph]);
                    }
                  }}
                />
                <Label
                  htmlFor={`graph-selector-${graph.graph_id}`}
                  className="w-full"
                >
                  <span
                    className={cn(
                      "w-full overflow-hidden text-ellipsis whitespace-nowrap"
                    )}
                  >
                    {graph.name}
                  </span>
                  {graph.auto_start && (
                    <span
                      className={cn(
                        "whitespace-nowrap text-muted-foreground/50 text-xs"
                      )}
                    >
                      {t("graph.auto-start")}
                    </span>
                  )}
                </Label>
                <div
                  className={cn(
                    "flex items-center gap-0.5",
                    "-translate-y-1/2 absolute top-1/2 right-0 transform",
                    "opacity-0 transition-opacity group-hover:opacity-100",
                    "overflow-hidden rounded bg-popover"
                  )}
                >
                  <Button
                    size="xs"
                    variant="link"
                    className={cn(
                      "opacity-0 transition-opacity group-hover:opacity-100"
                    )}
                    disabled={isLoading}
                    onClick={async () => {
                      try {
                        setIsLoading(true);
                        await postGraphsAutoStart({
                          auto_start: !graph.auto_start,
                          graph_id: graph.graph_id,
                        });
                        await mutateGraphs();
                        toast.success(t("graph.change-auto-start-success"), {
                          description: `${graph.name}`,
                        });
                      } catch (error) {
                        console.error(
                          "Failed to update auto-start setting:",
                          error
                        );
                        toast.error(t("graph.change-auto-start-failed"), {
                          description: `${graph.name}`,
                        });
                      } finally {
                        setIsLoading(false);
                      }
                    }}
                  >
                    {graph.auto_start
                      ? t("graph.disable-auto-start")
                      : t("graph.enable-auto-start")}
                  </Button>
                  <Button
                    size="xs"
                    variant="link"
                    disabled={isLoading}
                    className={cn(
                      "opacity-0 transition-opacity group-hover:opacity-100"
                    )}
                    onClick={() => {
                      setSelectedGraphs([graph]);
                      fitView();
                    }}
                  >
                    {t("graph.select-only")}
                  </Button>
                </div>
              </div>
            ))}
          </div>
        </React.Fragment>
      ))}
    </div>
  );
};
