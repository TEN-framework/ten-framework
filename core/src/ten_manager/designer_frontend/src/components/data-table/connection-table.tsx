//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import {
  type ColumnDef,
  type ColumnFiltersState,
  flexRender,
  getCoreRowModel,
  getFilteredRowModel,
  getSortedRowModel,
  type SortingState,
  useReactTable,
} from "@tanstack/react-table";
import {
  ArrowBigRightDashIcon,
  ArrowDownIcon,
  ArrowUpDown,
  ArrowUpIcon,
  MoreHorizontal,
  PuzzleIcon,
} from "lucide-react";
import * as React from "react";
import { Translation, useTranslation } from "react-i18next";
import { toast } from "sonner";
import { postDeleteConnection, useGraphs } from "@/api/services/graphs";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuLabel,
  // DropdownMenuSeparator,
  DropdownMenuTrigger,
} from "@/components/ui/dropdown-menu";
import {
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableHeader,
  TableRow,
} from "@/components/ui/table";
import { CustomNodeConnectionButton } from "@/flow/edge/button";
import {
  type IdentifierCustomNodeData,
  identifier2data,
} from "@/lib/identifier";
import { cn } from "@/lib/utils";
import { useDialogStore } from "@/store";
import { ECustomNodeType, type TCustomEdge } from "@/types/flow";
import { EConnectionType, type GraphInfo } from "@/types/graphs";

export type TConnection = {
  id: string;
  source: string;
  target: string;
  type?: EConnectionType;
  _meta: TCustomEdge;
  graph?: GraphInfo;
};

// eslint-disable-next-line react-refresh/only-export-components
export const commonConnectionColumns: ColumnDef<TConnection>[] = [
  {
    accessorKey: "id",
    header: () => (
      <Translation>
        {(t) => <div className="">{t("dataTable.no")}</div>}
      </Translation>
    ),
    cell: ({ row }) => {
      const index = row.index + 1;
      return <div className="font-medium">{index}</div>;
    },
  },
  {
    accessorKey: "type",
    header: ({ column }) => {
      return (
        <Button
          variant="ghost"
          size="th"
          onClick={() => column.toggleSorting(column.getIsSorted() === "asc")}
        >
          <Translation>
            {(t) => <div className="">{t("dataTable.type")}</div>}
          </Translation>
          {column.getIsSorted() === "asc" && (
            <ArrowUpIcon className="ml-2 h-4 w-4" />
          )}
          {column.getIsSorted() === "desc" && (
            <ArrowDownIcon className="ml-2 h-4 w-4" />
          )}
          {!column.getIsSorted() && <ArrowUpDown className="ml-2 h-4 w-4" />}
        </Button>
      );
    },
    cell: ({ row }) => {
      const type = row.getValue("type") as EConnectionType;
      if (!type) return null;
      return <ConnectionTypeWithBadge type={type} />;
    },
  },
  {
    accessorKey: "name",
    header: () => (
      <Translation>
        {(t) => <div className="">{t("dataTable.name")}</div>}
      </Translation>
    ),
    cell: ({ row }) => {
      const name = row.getValue("name") as string;
      if (!name) return null;
      return name;
    },
  },
];

export const ActionDropdownMenu = (props: { edge: TCustomEdge }) => {
  const { edge } = props;

  const { t } = useTranslation();

  const { appendDialog, removeDialog } = useDialogStore();

  const { mutate: mutateGraphs } = useGraphs();

  const disableDelete = React.useMemo(() => {
    return edge.data?.target?.type === ECustomNodeType.SELECTOR;
  }, [edge.data?.target?.type]);

  return (
    <DropdownMenu>
      <DropdownMenuTrigger asChild>
        <Button variant="ghost" className="h-8 w-8 p-0">
          <span className="sr-only">
            <Translation>
              {(t) => <div className="">{t("dataTable.openMenu")}</div>}
            </Translation>
          </span>
          <MoreHorizontal className="h-4 w-4" />
        </Button>
      </DropdownMenuTrigger>
      <DropdownMenuContent align="end" className="z-2000">
        <DropdownMenuLabel>
          <Translation>
            {(t) => <div className="">{t("dataTable.actions")}</div>}
          </Translation>
        </DropdownMenuLabel>
        {/* <DropdownMenuItem
      onClick={() => {
        toast.info("View Details", {
          description:
            "Source: " +
            source +
            ", Target: " +
            target +
            ", Type: " +
            type,
        });
      }}
    >
      <Translation>
        {(t) => <div className="">{t("dataTable.viewDetails")}</div>}
      </Translation>
    </DropdownMenuItem>
    <DropdownMenuSeparator /> */}
        <DropdownMenuItem
          disabled={disableDelete}
          className="bg-destructive/10 text-destructive"
          onClick={() => {
            const dialogId =
              edge.source +
              edge.target +
              edge.type +
              edge.id +
              "delete-popup-dialog";
            if (!edge.data || !edge.data?.graph) {
              return;
            }
            appendDialog({
              id: dialogId,
              variant: "destructive",
              title: t("action.confirm"),
              content: t("action.deleteConnectionConfirmation"),
              confirmLabel: t("action.delete"),
              cancelLabel: t("action.cancel"),
              onConfirm: async () => {
                if (!edge.data || !edge.data?.graph) {
                  return;
                }
                try {
                  await postDeleteConnection({
                    graph_id: edge.data.graph.graph_id,
                    src: {
                      app: edge.data.app,
                      [edge.data.source.type]: edge.data.source.name,
                    },
                    dest: {
                      app: edge.data.app,
                      [edge.data.target.type]: edge.data.target.name,
                    },
                    msg_type: edge.data.connectionType,
                    msg_names: edge.data.names || [edge.data.name],
                  });
                  toast.success(t("action.deleteConnectionSuccess"));
                  await mutateGraphs();
                } catch (error) {
                  console.error(error);
                  toast.error(t("action.deleteConnectionFailed"), {
                    description:
                      error instanceof Error ? error.message : "Unknown error",
                  });
                } finally {
                  removeDialog(dialogId);
                }
              },
              onCancel: async () => {
                removeDialog(dialogId);
              },
              postConfirm: async () => {},
            });
          }}
        >
          <Translation>
            {(t) => <div className="">{t("dataTable.delete")}</div>}
          </Translation>
        </DropdownMenuItem>
      </DropdownMenuContent>
    </DropdownMenu>
  );
};

// eslint-disable-next-line react-refresh/only-export-components
export const connectionColumns: ColumnDef<TConnection>[] = [
  ...commonConnectionColumns,
  {
    accessorKey: "source",
    header: () => (
      <Translation>
        {(t) => <div className="">{t("dataTable.source")}</div>}
      </Translation>
    ),
  },
  {
    accessorKey: "target",
    header: () => (
      <Translation>
        {(t) => <div className="">{t("dataTable.target")}</div>}
      </Translation>
    ),
  },
  {
    id: "actions",
    header: () => (
      <Translation>
        {(t) => <div className="">{t("dataTable.actions")}</div>}
      </Translation>
    ),
    cell: ({ row }) => {
      const connection = row.original;
      const { _meta } = connection;

      return <ActionDropdownMenu edge={_meta} />;
    },
  },
];

// eslint-disable-next-line react-refresh/only-export-components
export const extensionConnectionColumns1: ColumnDef<TConnection>[] = [
  ...commonConnectionColumns,
  {
    accessorKey: "downstream",
    header: () => (
      <Translation>
        {(t) => <div className="">{t("dataTable.downstream")}</div>}
      </Translation>
    ),
    cell: ({ row }) => {
      const downstream = row.getValue("downstream") as string;
      if (!downstream || !row.original.graph) return null;
      return (
        <div className="flex items-center">
          <PuzzleIcon className="me-1 h-4 w-4" />
          <ArrowBigRightDashIcon className="me-1 h-4 w-4" />
          <CustomNodeConnectionButton
            variant="outline"
            size="sm"
            data={{
              source: {
                name: identifier2data<IdentifierCustomNodeData>(downstream)
                  .name,
                type: identifier2data<IdentifierCustomNodeData>(downstream)
                  .type,
              },
              graph: row.original.graph,
            }}
          >
            <PuzzleIcon className="me-1 h-3 w-3" />
            <span className="text-xs">
              {identifier2data<IdentifierCustomNodeData>(downstream).name}
            </span>
          </CustomNodeConnectionButton>
        </div>
      );
    },
  },
  {
    id: "actions",
    header: () => (
      <Translation>
        {(t) => <div className="">{t("dataTable.actions")}</div>}
      </Translation>
    ),
    cell: ({ row }) => {
      const connection = row.original;
      const { _meta } = connection;

      return <ActionDropdownMenu edge={_meta} />;
    },
  },
];

// eslint-disable-next-line react-refresh/only-export-components
export const extensionConnectionColumns2: ColumnDef<TConnection>[] = [
  ...commonConnectionColumns,
  {
    accessorKey: "upstream",
    header: () => (
      <Translation>
        {(t) => <div className="">{t("dataTable.upstream")}</div>}
      </Translation>
    ),
    cell: ({ row }) => {
      const upstream = row.getValue("upstream") as string;
      if (!upstream || !row.original.graph) return null;
      return (
        <div className="flex items-center">
          <CustomNodeConnectionButton
            variant="outline"
            size="sm"
            data={{
              source: {
                name: identifier2data<IdentifierCustomNodeData>(upstream).name,
                type: identifier2data<IdentifierCustomNodeData>(upstream).type,
              },
              graph: row.original.graph,
            }}
          >
            <PuzzleIcon className="me-1 h-3 w-3" />
            <span className="text-xs">
              {identifier2data<IdentifierCustomNodeData>(upstream).name}
            </span>
          </CustomNodeConnectionButton>
          <ArrowBigRightDashIcon className="ms-1 h-4 w-4" />
          <PuzzleIcon className="ms-1 h-4 w-4" />
        </div>
      );
    },
  },
  {
    id: "actions",
    header: () => (
      <Translation>
        {(t) => <div className="">{t("dataTable.actions")}</div>}
      </Translation>
    ),
    cell: ({ row }) => {
      const connection = row.original;
      const { _meta } = connection;

      return <ActionDropdownMenu edge={_meta} />;
    },
  },
];

interface DataTableProps<TData, TValue> {
  columns: ColumnDef<TData, TValue>[];
  data: TData[];
}

export function DataTable<TData, TValue>({
  columns,
  data,
  className,
}: DataTableProps<TData, TValue> & { className?: string }) {
  const [sorting, setSorting] = React.useState<SortingState>([]);
  const [columnFilters, setColumnFilters] = React.useState<ColumnFiltersState>(
    []
  );

  const { t } = useTranslation();

  const table = useReactTable({
    data,
    columns,
    getCoreRowModel: getCoreRowModel(),
    onSortingChange: setSorting,
    getSortedRowModel: getSortedRowModel(),
    onColumnFiltersChange: setColumnFilters,
    getFilteredRowModel: getFilteredRowModel(),
    state: {
      sorting,
      columnFilters,
    },
  });

  return (
    <div className={cn("", className)}>
      <Table>
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
    </div>
  );
}

// eslint-disable-next-line react-refresh/only-export-components
export const connectionTypeBadgeStyle = {
  [EConnectionType.CMD]: "bg-blue-100 text-blue-800 border-blue-200",
  [EConnectionType.DATA]: "bg-green-100 text-green-800 border-green-200",
  [EConnectionType.AUDIO_FRAME]:
    "bg-purple-100 text-purple-800 border-purple-200",
  [EConnectionType.VIDEO_FRAME]: "bg-red-100 text-red-800 border-red-200",
};

export function ConnectionTypeWithBadge({
  type,
  className,
}: {
  type: EConnectionType;
  className?: string;
}) {
  return (
    <Badge
      variant="outline"
      className={cn(connectionTypeBadgeStyle[type], className)}
    >
      {type}
    </Badge>
  );
}
