"use client";

import { type ReactNode, useEffect, useRef } from "react";
import {
  getOptionsFromLocal,
  getRandomChannel,
  getRandomUserId,
  getTrulienceSettingsFromLocal,
  useAppDispatch,
  useAppSelector,
} from "@/common";
import { useGraphs } from "@/common/hooks";
import {
  addChatItem,
  fetchGraphDetails,
  reset,
  setAgentPhase,
  setOptions,
  setSelectedGraphId,
  setTrulienceSettings,
} from "@/store/reducers/global";
import { EMessageDataType, EMessageType } from "@/types";
import { openclawGateway } from "@/openclaw/gatewayManager";

interface AuthInitializerProps {
  children: ReactNode;
}

const AuthInitializer = (props: AuthInitializerProps) => {
  const { children } = props;
  const dispatch = useAppDispatch();
  const { initialize } = useGraphs();
  const selectedGraphId = useAppSelector(
    (state) => state.global.selectedGraphId
  );
  const graphList = useAppSelector((state) => state.global.graphList);
  const urlParamApplied = useRef(false);

  useEffect(() => {
    if (typeof window !== "undefined") {
      const options = getOptionsFromLocal();
      const trulienceSettings = getTrulienceSettingsFromLocal();
      initialize();
      if (options?.channel) {
        dispatch(reset());
        dispatch(setOptions(options));
        dispatch(setTrulienceSettings(trulienceSettings));
      } else {
        dispatch(reset());
        dispatch(
          setOptions({
            channel: getRandomChannel(),
            userId: getRandomUserId(),
          })
        );
      }
    }
  }, [dispatch, initialize]);

  useEffect(() => {
    const handleAgentPhase = (phase: string) => {
      dispatch(setAgentPhase(phase));
    };
    const handleOpenclawResponse = (text: string, timestamp: number) => {
      dispatch(
        addChatItem({
          userId: "openclaw",
          text,
          type: EMessageType.AGENT,
          data_type: EMessageDataType.OPENCLAW,
          isFinal: true,
          time: timestamp,
        })
      );
    };

    openclawGateway.on("agentPhase", handleAgentPhase);
    openclawGateway.on("openclawResponse", handleOpenclawResponse);

    return () => {
      openclawGateway.off("agentPhase", handleAgentPhase);
      openclawGateway.off("openclawResponse", handleOpenclawResponse);
    };
  }, [dispatch]);

  // Check URL params for graph selection on initial load only
  useEffect(() => {
    if (urlParamApplied.current) return;
    if (typeof window !== "undefined" && graphList.length > 0) {
      const urlParams = new URLSearchParams(window.location.search);
      const graphParam = urlParams.get("graph");
      if (graphParam) {
        // Find graph by name (frontend uses UUIDs for graph_id, API uses name)
        const graph = graphList.find((g) => g.name === graphParam);
        if (graph) {
          const graphId = graph.graph_id || graph.name;
          dispatch(setSelectedGraphId(graphId));
          urlParamApplied.current = true;
        }
      }
    }
  }, [graphList, dispatch]);

  useEffect(() => {
    if (selectedGraphId) {
      const graph = graphList.find((g) => g.graph_id === selectedGraphId);
      if (!graph) {
        return;
      }
      dispatch(fetchGraphDetails(graph));
    }
  }, [selectedGraphId, graphList, dispatch]); // Automatically fetch details when `selectedGraphId` changes

  return children;
};

export default AuthInitializer;
