//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as React from "react";
import { useTranslation } from "react-i18next";
import { toast } from "sonner";
import {
  SortingState,
  getSortedRowModel,
  ColumnDef,
  flexRender,
  getCoreRowModel,
  useReactTable,
} from "@tanstack/react-table";
import { ArrowUpDown } from "lucide-react";

import {
  Select,
  SelectContent,
  SelectGroup,
  SelectItem,
  SelectLabel,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/Select";
import { Checkbox } from "@/components/ui/Checkbox";
import { Badge } from "@/components/ui/Badge";
import {
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableHeader,
  TableRow,
} from "@/components/ui/Table";
import { Label } from "@/components/ui/Label";
import { Button } from "@/components/ui/Button";
import { SpinnerLoading } from "@/components/Status/Loading";
import { useGraphs } from "@/api/services/graphs";
import { useApps } from "@/api/services/apps";
import { useWidgetStore, useFlowStore, useAppStore } from "@/store";
import { cn } from "@/lib/utils";

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

  const { graphs = [], error, isLoading } = useGraphs();

  const handleOk = () => {
    removeWidget(widget.widget_id);
  };

  const handleSelectGraph = async (graph: IGraph) => {
    updateCurrentWorkspace({
      app: selectedApp,
      graph,
    });
    try {
      const { nodes: layoutedNodes, edges: layoutedEdges } =
        await resetNodesAndEdgesByGraph(graph);

      setNodesAndEdges(layoutedNodes, layoutedEdges);

      toast.success(t("popup.selectGraph.updateSuccess"), {
        description: (
          <>
            <p>{`${t("popup.selectGraph.app")}: ${selectedApp?.base_dir}`}</p>
            <p>{`${t("popup.selectGraph.graph")}: ${graph.name}`}</p>
          </>
        ),
      });
    } catch (err: unknown) {
      console.error(err);
      toast.error("Failed to load graph.");
    } finally {
      // removeWidget(widget.widget_id);
    }
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
        <div className="h-full overflow-y-auto">
          <div className="rounded-md border">
            <GraphSelectTable
              items={graphs?.filter(
                (graph) => graph.base_dir === selectedApp?.base_dir
              )}
              onSelect={handleSelectGraph}
              className="pointer-events-auto"
            />
          </div>
        </div>
      )}
      <div className="flex mt-auto justify-end gap-2">
        <Button variant="default" onClick={handleOk}>
          {t("action.ok")}
        </Button>
      </div>
    </div>
  );
};

const GraphSelectTable = (props: {
  items?: IGraph[];
  onSelect?: (item: IGraph) => void;
  className?: string;
}) => {
  const { items = [], onSelect, className } = props;
  const [sorting, setSorting] = React.useState<SortingState>([]);

  const { t } = useTranslation();
  const { currentWorkspace } = useAppStore();

  const columns: ColumnDef<IGraph>[] = [
    {
      accessorKey: "name",
      // header: t("dataTable.name"),
      header: ({ column }) => {
        return (
          <Button
            variant="ghost"
            onClick={() => column.toggleSorting(column.getIsSorted() === "asc")}
          >
            {t("dataTable.name")}
            <ArrowUpDown className="ml-2 h-4 w-4" />
          </Button>
        );
      },
      cell: ({ row, getValue }) => {
        const value = getValue() as string;
        const isCurrent = row.original.uuid === currentWorkspace?.graph?.uuid;

        return (
          <div className="flex items-center">
            <span className="text-sm">{value}</span>
            {isCurrent && (
              <Badge className="ml-2" variant="outline">
                {t("action.current")}
              </Badge>
            )}
          </div>
        );
      },
    },
    {
      accessorKey: "auto_start",
      header: t("action.autoStart"),
      cell: ({ getValue }) => {
        const value = getValue() as boolean;
        return (
          <div className="flex items-center">
            <Checkbox disabled checked={value} />
          </div>
        );
      },
    },
    {
      header: t("dataTable.actions"),
      cell: ({ row }) => {
        const isCurrent = row.original.uuid === currentWorkspace?.graph?.uuid;

        return (
          <div className="flex items-center">
            <Button
              size="sm"
              variant="outline"
              disabled={isCurrent}
              onClick={() => {
                const graph = row.original as IGraph;
                onSelect?.(graph);
              }}
            >
              {isCurrent
                ? t("popup.selectGraph.selected")
                : t("popup.selectGraph.select")}
            </Button>
          </div>
        );
      },
    },
  ];

  const table = useReactTable<IGraph>({
    data: items,
    columns,
    getCoreRowModel: getCoreRowModel(),
    onSortingChange: setSorting,
    getSortedRowModel: getSortedRowModel(),
    state: {
      sorting,
    },
  });

  return (
    <>
      <Table className={cn("w-full caption-bottom text-sm", className)}>
        <TableHeader>
          {table.getHeaderGroups().map((headerGroup) => (
            <TableRow key={headerGroup.id}>
              {headerGroup.headers.map((header) => {
                return (
                  <TableHead key={header.id}>
                    {header.isPlaceholder
                      ? null
                      : flexRender(
                          header.column.columnDef.header,
                          header.getContext()
                        )}
                  </TableHead>
                );
              })}
            </TableRow>
          ))}
        </TableHeader>
        <TableBody>
          {table.getRowModel().rows?.length ? (
            table.getRowModel().rows.map((row) => (
              <TableRow
                key={row.id}
                data-state={row.getIsSelected() && "selected"}
              >
                {row.getVisibleCells().map((cell) => (
                  <TableCell key={cell.id}>
                    {flexRender(cell.column.columnDef.cell, cell.getContext())}
                  </TableCell>
                ))}
              </TableRow>
            ))
          ) : (
            <TableRow>
              <TableCell colSpan={columns.length} className="h-24 text-center">
                {t("dataTable.noResults")}
              </TableCell>
            </TableRow>
          )}
        </TableBody>
      </Table>
    </>
  );
};
