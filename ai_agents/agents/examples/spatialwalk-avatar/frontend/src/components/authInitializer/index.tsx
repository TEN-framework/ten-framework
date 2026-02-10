"use client";

import { type ReactNode, useEffect, useRef } from "react";
import { toast } from "sonner";
import {
  getSpatialwalkUrlConfig,
  getOptionsFromLocal,
  getRandomChannel,
  getRandomUserId,
  getSpatialwalkSettingsFromLocal,
  useAppDispatch,
  useAppSelector,
  validateSpatialwalkRequiredConfig,
} from "@/common";
import { useGraphs } from "@/common/hooks";
import {
  fetchGraphDetails,
  reset,
  setOptions,
  setSelectedGraphId,
  setSpatialwalkSettings,
} from "@/store/reducers/global";

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
  const spatialwalkWarningShown = useRef(false);

  useEffect(() => {
    if (typeof window !== "undefined") {
      const options = getOptionsFromLocal();
      const spatialwalkSettings = getSpatialwalkSettingsFromLocal();
      const validation = validateSpatialwalkRequiredConfig(
        getSpatialwalkUrlConfig()
      );
      initialize();
      if (options && options.channel) {
        dispatch(reset());
        dispatch(setOptions(options));
        dispatch(setSpatialwalkSettings(spatialwalkSettings));
      } else {
        dispatch(reset());
        dispatch(
          setOptions({
            channel: getRandomChannel(),
            userId: getRandomUserId(),
          })
        );
        dispatch(setSpatialwalkSettings(spatialwalkSettings));
      }
      if (!spatialwalkWarningShown.current && !validation.isValid) {
        spatialwalkWarningShown.current = true;
        toast.error("Spatialwalk Settings", {
          description: validation.message,
        });
      }
    }
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
