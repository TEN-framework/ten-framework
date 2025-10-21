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
