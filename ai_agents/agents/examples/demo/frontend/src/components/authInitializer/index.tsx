"use client";

import { type ReactNode, useEffect } from "react";
import {
  genRandomString,
  getOptionsFromLocal,
  getRandomChannel,
  getRandomUserId,
  useAppDispatch,
} from "@/common";
import {
  reset,
  setAgentSettings,
  setCozeSettings,
  setDifySettings,
  setOceanBaseSettings,
  setOptions,
} from "@/store/reducers/global";

interface AuthInitializerProps {
  children: ReactNode;
}

const AuthInitializer = (props: AuthInitializerProps) => {
  const { children } = props;
  const dispatch = useAppDispatch();

  useEffect(() => {
    if (typeof window !== "undefined") {
      const data = getOptionsFromLocal();
      if (data?.options?.channel) {
        dispatch(reset());
        dispatch(setOptions(data.options));
        dispatch(setAgentSettings(data.settings));
        dispatch(setCozeSettings(data.cozeSettings));
        dispatch(setDifySettings(data.difySettings));
        dispatch(setOceanBaseSettings(data.oceanbaseSettings));
      } else {
        const newOptions = {
          userName: genRandomString(8),
          channel: getRandomChannel(),
          userId: getRandomUserId(),
        };
        dispatch(setOptions(newOptions));
      }
    }
  }, [dispatch]);

  return children;
};

export default AuthInitializer;
