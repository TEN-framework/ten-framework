//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { create } from "zustand";
import { devtools } from "zustand/middleware";
import type { TCommonNode, TCustomEdge } from "@/types/flow";
import { EConnectionType } from "@/types/graphs";

export interface IFlowStore {
  nodes: TCommonNode[];
  edges: TCustomEdge[];
  cdavInfo: Map<
    string,
    {
      source: TCommonNode;
      target: TCommonNode;
      relatedEdges: TCustomEdge[];
      connectionCounts: {
        cmd: number;
        data: number;
        audio: number;
        video: number;
      };
    }
  >;

  setNodes: (nodes: TCommonNode[]) => void;
  setEdges: (edges: TCustomEdge[]) => void;
  setNodesAndEdges: (nodes: TCommonNode[], edges: TCustomEdge[]) => void;
}

export const useFlowStore = create<IFlowStore>()(
  devtools((set, get) => ({
    nodes: [],
    edges: [],
    cdavInfo: new Map(),

    setNodes: (nodes: TCommonNode[]) => {
      set({ nodes });
      // Recalculate CDAV info when nodes change
      const { edges } = get();
      const cdavInfo = calculateCDAVInfo(nodes, edges);
      set({ cdavInfo });
    },

    setEdges: (edges: TCustomEdge[]) => {
      set({ edges });
      // Recalculate CDAV info when edges change
      const { nodes } = get();
      const cdavInfo = calculateCDAVInfo(nodes, edges);
      set({ cdavInfo });
    },

    setNodesAndEdges: (nodes: TCommonNode[], edges: TCustomEdge[]) => {
      set({ nodes, edges });
      const cdavInfo = calculateCDAVInfo(nodes, edges);
      set({ cdavInfo });
    },
  }))
);

function calculateCDAVInfo(nodes: TCommonNode[], edges: TCustomEdge[]) {
  const cdavInfo = new Map();

  edges.forEach((edge) => {
    const source = nodes.find((n) => n.id === edge.source);
    const target = nodes.find((n) => n.id === edge.target);
    if (!source || !target) return;

    const relatedEdges = edges.filter(
      (e) => e.source === edge.source && e.target === edge.target
    );

    let cmdCount = 0;
    let dataCount = 0;
    let audioCount = 0;
    let videoCount = 0;

    relatedEdges.forEach((e) => {
      switch (e.data?.connectionType) {
        case EConnectionType.CMD:
          cmdCount++;
          break;
        case EConnectionType.DATA:
          dataCount++;
          break;
        case EConnectionType.AUDIO_FRAME:
          audioCount++;
          break;
        case EConnectionType.VIDEO_FRAME:
          videoCount++;
          break;
      }
    });

    cdavInfo.set(edge.id, {
      source,
      target,
      relatedEdges,
      connectionCounts: {
        cmd: cmdCount,
        data: dataCount,
        audio: audioCount,
        video: videoCount,
      },
    });
  });

  return cdavInfo;
}
