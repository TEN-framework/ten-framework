//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as React from "react";
import { useTranslation } from "react-i18next";
import { toast } from "sonner";
import { CheckIcon } from "lucide-react";

import {
  Select,
  SelectContent,
  SelectGroup,
  SelectItem,
  SelectLabel,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/Select";
import { Label } from "@/components/ui/Label";
import { Button } from "@/components/ui/Button";
import { SpinnerLoading } from "@/components/Status/Loading";
import { useGraphs } from "@/api/services/graphs";
import { useApps } from "@/api/services/apps";
import { useWidgetStore, useFlowStore, useAppStore } from "@/store";

import { resetNodesAndEdgesByGraph } from "@/components/Widget/GraphsWidget";
import { IWidget } from "@/types/widgets";
import { type IApp } from "@/types/apps";
import { type IGraph } from "@/types/graphs";

export const GraphSelectPopupTitle = () => {
  const { t } = useTranslation();
  return t("popup.selectGraph.title");
};

export const GraphSelectPopupContent = (props: { widget: IWidget }) => {
  const { widget } = props;

  const { t } = useTranslation();

  const {
    data: loadedApps,
    isLoading: isLoadingApps,
    error: errorApps,
  } = useApps();
  const { removeWidget } = useWidgetStore();
  const { setNodesAndEdges } = useFlowStore();
  const { currentWorkspace, updateCurrentWorkspace } = useAppStore();

  const [selectedApp, setSelectedApp] = React.useState<IApp | null>(
    currentWorkspace?.app ?? loadedApps?.app_info?.[0] ?? null
  );
  const [tmpSelectedGraph, setTmpSelectedGraph] = React.useState<IGraph | null>(
    currentWorkspace.graph ?? null
  );

  const { graphs = [], error, isLoading } = useGraphs();

  const handleSelectApp = (app?: IApp | null) => {
    setSelectedApp(app ?? null);
    setTmpSelectedGraph(null);
  };

  const handleSelectGraph = (graph: IGraph) => async () => {
    updateCurrentWorkspace({
      app: selectedApp,
      graph,
    });
    try {
      const { nodes: layoutedNodes, edges: layoutedEdges } =
        await resetNodesAndEdgesByGraph(graph);

      setNodesAndEdges(layoutedNodes, layoutedEdges);
    } catch (err: unknown) {
      console.error(err);
      toast.error("Failed to load graph.");
    } finally {
      removeWidget(widget.widget_id);
    }
  };

  const handleCancel = () => {
    removeWidget(widget.widget_id);
    handleSelectApp(currentWorkspace.app);
    setTmpSelectedGraph(currentWorkspace.graph);
  };

  const handleSave = async () => {
    if (!tmpSelectedGraph) {
      updateCurrentWorkspace({
        app: selectedApp,
        graph: null,
      });
      setNodesAndEdges([], []);
      toast.success(t("popup.selectGraph.updateSuccess"), {
        description: (
          <>
            <p>{`${t("popup.selectGraph.app")}: ${selectedApp?.base_dir}`}</p>
          </>
        ),
      });
    } else {
      await handleSelectGraph(tmpSelectedGraph)();
      toast.success(t("popup.selectGraph.updateSuccess"), {
        description: (
          <>
            <p>{`${t("popup.selectGraph.app")}: ${selectedApp?.base_dir}`}</p>
            <p>{`${t("popup.selectGraph.graph")}: ${tmpSelectedGraph.name}`}</p>
          </>
        ),
      });
    }
    removeWidget(widget.widget_id);
  };

  React.useEffect(() => {
    if (error instanceof Error) {
      toast.error(`Failed to fetch graphs: ${error.message}`);
    } else if (error) {
      toast.error("An unknown error occurred.");
    }
    if (errorApps instanceof Error) {
      toast.error(`Failed to fetch apps: ${errorApps.message}`);
    } else if (errorApps) {
      toast.error("An unknown error occurred.");
    }
  }, [error, errorApps]);

  return (
    <div className="flex flex-col gap-2 w-full h-full">
      <Label>{t("popup.selectGraph.app")}</Label>
      {isLoadingApps ? (
        <SpinnerLoading
          className="w-full h-full"
          svgProps={{ className: "size-10" }}
        />
      ) : (
        <Select
          onValueChange={(value) => {
            const app = loadedApps?.app_info?.find(
              (app) => app.base_dir === value
            );
            if (app) {
              setSelectedApp(app);
            }
          }}
          value={selectedApp?.base_dir ?? undefined}
        >
          <SelectTrigger className="w-full">
            <SelectValue placeholder={t("header.menuGraph.selectLoadedApp")} />
          </SelectTrigger>
          <SelectContent>
            <SelectGroup>
              <SelectLabel>{t("header.menuGraph.selectLoadedApp")}</SelectLabel>
              {loadedApps?.app_info?.map((app) => (
                <SelectItem key={app.base_dir} value={app.base_dir}>
                  {app.base_dir}
                </SelectItem>
              ))}
            </SelectGroup>
          </SelectContent>
        </Select>
      )}
      <Label>{t("popup.selectGraph.graph")}</Label>
      {isLoading ? (
        <>
          <SpinnerLoading
            className="w-full h-full"
            svgProps={{ className: "size-10" }}
          />
        </>
      ) : (
        <ul className="flex flex-col gap-1 h-fit overflow-y-auto">
          <Button
            asChild
            key={"null"}
            className="justify-start cursor-pointer"
            variant="ghost"
            onClick={() => setTmpSelectedGraph(null)}
          >
            <li className="w-full">
              {tmpSelectedGraph === null ? (
                <CheckIcon className="size-4" />
              ) : (
                <div className="size-4" />
              )}
              <span className="text-sm">
                {t("popup.selectGraph.unspecified")}
              </span>
            </li>
          </Button>
          {graphs
            ?.filter((graph) => graph.base_dir === selectedApp?.base_dir)
            ?.map((graph) => (
              <Button
                asChild
                key={graph.name}
                className="justify-start cursor-pointer"
                variant="ghost"
                onClick={() => setTmpSelectedGraph(graph)}
              >
                <li className="w-full">
                  {tmpSelectedGraph?.uuid === graph.uuid ? (
                    <CheckIcon className="size-4" />
                  ) : (
                    <div className="size-4" />
                  )}
                  <span className="text-sm">{graph.name}</span>
                  {graph.auto_start ? (
                    <span className="text-xs">({t("action.autoStart")})</span>
                  ) : null}
                  {selectedApp?.base_dir === currentWorkspace.app?.base_dir &&
                  graph.name === currentWorkspace.graph?.name ? (
                    <span className="text-xs">({t("action.current")})</span>
                  ) : null}
                </li>
              </Button>
            ))}
        </ul>
      )}
      <div className="flex justify-end gap-2">
        <Button variant="outline" onClick={handleCancel}>
          {t("action.cancel")}
        </Button>
        <Button onClick={handleSave}>{t("action.save")}</Button>
      </div>
    </div>
  );
};
