//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as React from "react";
import { useTranslation } from "react-i18next";
import { BlocksIcon, ArrowBigRightDashIcon, XIcon } from "lucide-react";

import { Button } from "@/components/ui/Button";
import { Tabs, TabsList, TabsTrigger } from "@/components/ui/Tabs";
import { Badge } from "@/components/ui/Badge";
import { useFlowStore } from "@/store/flow";
import {
  DataTable as ConnectionDataTable,
  connectionColumns,
  extensionConnectionColumns1,
  extensionConnectionColumns2,
} from "@/components/DataTable/ConnectionTable";
import { dispatchCustomNodeActionPopup } from "@/utils/events";

import type {
  ICustomConnectionWidgetData,
  ICustomConnectionWidget,
} from "@/types/widgets";
import { EConnectionType } from "@/types/graphs";

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
  const { source, target, filters } = widget.metadata;

  return (
    <div className="flex flex-col gap-2 w-full h-full">
      {source && target && (
        <EdgeInfoContent source={source} target={target} filters={filters} />
      )}
      {source && !target && (
        <CustomNodeConnContent source={source} filters={filters} />
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
}) {
  const { source, target, filters: initialFilters } = props;
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
        source: e.source,
        target: e.target,
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
    return [relatedEdges, rows];
  }, [edges, source, target, filters]);

  const handleRemoveFilter = (label: string) => {
    setFilters(filters.filter((f) => f.label !== label));
  };

  return (
    <>
      <div className="flex w-full items-center gap-2">
        <Button
          variant="outline"
          size="lg"
          onClick={() =>
            dispatchCustomNodeActionPopup({
              action: "connections",
              source,
            })
          }
        >
          <BlocksIcon className="w-4 h-4" />
          <span>{source}</span>
        </Button>
        <ArrowBigRightDashIcon className="w-6 h-6" />
        <Button
          variant="outline"
          size="lg"
          onClick={() =>
            dispatchCustomNodeActionPopup({
              action: "connections",
              source: target,
            })
          }
        >
          <BlocksIcon className="w-4 h-4" />
          <span>{target}</span>
        </Button>
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
}) {
  const { source, filters: initialFilters } = props;
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
    const relatedEdges = edges.filter((e) =>
      flowDirection === "upstream" ? e.target === source : e.source === source
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
  }, [flowDirection, edges, source, filters]);

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
                className="[&>svg]:size-3 h-4 w-4 cursor-pointer"
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
