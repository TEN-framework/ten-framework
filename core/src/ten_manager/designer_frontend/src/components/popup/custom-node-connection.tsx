//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import { ArrowBigRightDashIcon, PuzzleIcon, XIcon } from "lucide-react";
import * as React from "react";
import { useTranslation } from "react-i18next";
import {
  DataTable as ConnectionDataTable,
  connectionColumns,
  extensionConnectionColumns1,
  extensionConnectionColumns2,
} from "@/components/data-table/connection-table";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { Tabs, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { CustomNodeConnectionButton } from "@/flow/edge/button";
import { identifier2data, type TCustomNodeData } from "@/lib/identifier";
import { useFlowStore } from "@/store/flow";
import type { EConnectionType, IGraph } from "@/types/graphs";
import type {
  ICustomConnectionWidget,
  ICustomConnectionWidgetData,
} from "@/types/widgets";

const SUPPORTED_FILTERS = ["type"];

export const CustomNodeConnPopupTitle = (props: {
  source: string;
  target?: string;
}) => {
  const { source, target } = props;
  const { t } = useTranslation();

  const titleMemo = React.useMemo(() => {
    if (source && !target) {
      return t("popup.customNodeConn.srcTitle", { source });
    }
    if (source && target) {
      return t("popup.customNodeConn.connectTitle", { source, target });
    }
    return t("popup.customNodeConn.title");
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [source, target]);

  return titleMemo;
};

export const CustomNodeConnPopupContent = (props: {
  widget: ICustomConnectionWidget;
}) => {
  const { widget } = props;
  const { source, target, filters, graph } = widget.metadata;

  return (
    <div className="flex h-full w-full flex-col gap-2">
      {source && target && (
        <EdgeInfoContent
          source={source}
          target={target}
          filters={filters}
          graph={graph}
        />
      )}
      {source && !target && (
        <CustomNodeConnContent
          source={source}
          filters={filters}
          graph={graph}
        />
      )}
    </div>
  );
};

export interface CustomNodeConnPopupProps extends ICustomConnectionWidgetData {
  onClose?: () => void;
}

function EdgeInfoContent(props: {
  source: string;
  target: string;
  filters?: {
    type?: EConnectionType;
    source?: boolean;
    target?: boolean;
  };
  graph: IGraph;
}) {
  const { source, target, filters: initialFilters, graph } = props;
  const [filters, setFilters] = React.useState<TFilterItem[]>(() => {
    if (!initialFilters) return [];
    return Object.entries(initialFilters)
      .map(([key, value]) => ({
        label: key,
        value,
      }))
      .filter((item) => SUPPORTED_FILTERS.includes(item.label));
  });

  const { edges } = useFlowStore();

  const [, rowsMemo] = React.useMemo(() => {
    const relatedEdges = edges.filter(
      (e) => e.source === source && e.target === target
    );
    const rows = relatedEdges
      .map((e) => ({
        id: e.id,
        type: e.data?.connectionType,
        name: e.data?.name,
        source: identifier2data<TCustomNodeData>(e.source).name,
        target: identifier2data<TCustomNodeData>(e.target).name,
        _meta: e,
        graph: e.data?.graph,
      }))
      .filter((row) => {
        const enabledFilters = filters.filter((i) =>
          SUPPORTED_FILTERS.includes(i.label)
        );
        return enabledFilters.every(
          (f) => row[f.label as keyof typeof row] === f.value
        );
      });
    return [relatedEdges, rows];
  }, [edges, source, target, filters]);
  const [prettySource, prettyTarget] = React.useMemo(() => {
    return [
      identifier2data<TCustomNodeData>(source).name,
      identifier2data<TCustomNodeData>(target).name,
    ];
  }, [source, target]);

  const handleRemoveFilter = (label: string) => {
    setFilters(filters.filter((f) => f.label !== label));
  };

  return (
    <>
      <div className="flex w-full items-center gap-2">
        <CustomNodeConnectionButton
          variant="outline"
          size="lg"
          data={{
            source: prettySource,
            graph,
          }}
        >
          <PuzzleIcon className="h-4 w-4" />
          <span>{prettySource}</span>
        </CustomNodeConnectionButton>
        <ArrowBigRightDashIcon className="h-6 w-6" />
        <CustomNodeConnectionButton
          variant="outline"
          size="lg"
          data={{
            source: prettyTarget,
            graph,
          }}
        >
          <PuzzleIcon className="h-4 w-4" />
          <span>{prettyTarget}</span>
        </CustomNodeConnectionButton>
      </div>
      <Filters
        items={filters}
        onRemove={(label) => handleRemoveFilter(label)}
      />
      <ConnectionDataTable
        columns={connectionColumns}
        data={rowsMemo}
        className="overflow-y-auto"
      />
    </>
  );
}

function CustomNodeConnContent(props: {
  source: string;
  filters?: {
    type?: EConnectionType;
    source?: boolean;
    target?: boolean;
  };
  graph: IGraph;
}) {
  const { source, filters: initialFilters, graph } = props;
  const [filters, setFilters] = React.useState<TFilterItem[]>(() => {
    if (!initialFilters) return [];
    return Object.entries(initialFilters)
      .map(([key, value]) => ({
        label: key,
        value,
      }))
      .filter((item) => SUPPORTED_FILTERS.includes(item.label));
  });
  const [flowDirection, setFlowDirection] = React.useState<
    "upstream" | "downstream"
  >(() => {
    if (initialFilters?.source) return "downstream";
    if (initialFilters?.target) return "upstream";
    return "upstream";
  });

  const { t } = useTranslation();

  const { edges } = useFlowStore();

  const [rowsMemo] = React.useMemo(() => {
    const relatedEdges = edges
      .filter((e) => e.data?.graph?.uuid === graph.uuid)
      ?.filter((e) =>
        flowDirection === "upstream"
          ? identifier2data<TCustomNodeData>(e.target).name === source
          : identifier2data<TCustomNodeData>(e.source).name === source
      );
    const rows = relatedEdges
      .map((e) => ({
        id: e.id,
        type: e.data?.connectionType,
        name: e.data?.name,
        upstream: flowDirection === "upstream" ? e.source : e.target,
        downstream: flowDirection === "upstream" ? e.source : e.target,
        _meta: e,
      }))
      .filter((row) => {
        const enabledFilters = filters.filter((i) =>
          SUPPORTED_FILTERS.includes(i.label)
        );
        return enabledFilters.every(
          (f) => row[f.label as keyof typeof row] === f.value
        );
      });
    return [rows, relatedEdges];
  }, [edges, graph.uuid, flowDirection, source, filters]);

  const handleRemoveFilter = (label: string) => {
    setFilters(filters.filter((f) => f.label !== label));
  };

  return (
    <>
      <Tabs
        value={flowDirection}
        onValueChange={(value) =>
          setFlowDirection(value as "upstream" | "downstream")
        }
        className=""
      >
        <TabsList>
          <TabsTrigger value="upstream">{t("action.upstream")}</TabsTrigger>
          <TabsTrigger value="downstream">{t("action.downstream")}</TabsTrigger>
        </TabsList>
      </Tabs>
      <Filters
        items={filters}
        onRemove={(label) => handleRemoveFilter(label)}
      />
      <ConnectionDataTable
        columns={
          flowDirection === "upstream"
            ? extensionConnectionColumns2
            : extensionConnectionColumns1
        }
        data={rowsMemo.map((row) => ({
          ...row,
          source: row.upstream,
          target: row.downstream,
          graph: graph,
        }))}
        className="overflow-y-auto"
      />
    </>
  );
}

type TFilterItem = {
  label: string;
  value: boolean | number | string;
};

const Filters = (props: {
  items: TFilterItem[];
  onRemove?: (label: string) => void;
}) => {
  const { items, onRemove } = props;

  const { t } = useTranslation();

  if (items.length === 0) return null;

  return (
    <div className="flex items-center gap-2">
      <span>{t("popup.customNodeConn.filters")}</span>
      <ul className="flex flex-wrap gap-2">
        {items.map((item) => (
          <li key={item.label} className="flex">
            <Badge variant="secondary" className="flex items-center gap-1">
              <span>
                {item.label}: {item.value}
              </span>
              <Button
                variant="ghost"
                size="icon"
                className="h-4 w-4 cursor-pointer [&>svg]:size-3"
                disabled={!onRemove}
                onClick={() => onRemove?.(item.label)}
              >
                <XIcon />
              </Button>
            </Badge>
          </li>
        ))}
      </ul>
    </div>
  );
};
