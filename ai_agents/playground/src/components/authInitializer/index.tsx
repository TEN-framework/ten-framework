"use client";

import { type ReactNode, useEffect } from "react";
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
  fetchGraphDetails,
  reset,
  setOptions,
  setSelectedGraphId,
  setTrulienceSettings,
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

  useEffect(() => {
    if (typeof window !== "undefined") {
      const options = getOptionsFromLocal();
      const trulienceSettings = getTrulienceSettingsFromLocal();
      initialize();
      if (options && options.channel) {
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
  }, [dispatch]);

  // Check URL for ?graph= parameter and auto-select graph
  useEffect(() => {
    if (typeof window !== "undefined" && graphList.length > 0 && !selectedGraphId) {
      const urlParams = new URLSearchParams(window.location.search);
      const graphParam = urlParams.get("graph");

      if (graphParam) {
        // Find graph by name or graph_id
        const matchingGraph = graphList.find(
          (g) => g.name === graphParam || g.graph_id === graphParam
        );
        if (matchingGraph) {
          dispatch(setSelectedGraphId(matchingGraph.graph_id));
        }
      }
    }
  }, [graphList, dispatch, selectedGraphId]);

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
