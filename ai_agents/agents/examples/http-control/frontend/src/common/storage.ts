import type { IOptions, ITrulienceSettings } from "@/types";
import {
  DEFAULT_OPTIONS,
  DEFAULT_TRULIENCE_OPTIONS,
  OPTIONS_KEY,
  TRULIENCE_SETTINGS_KEY,
} from "./constant";

export const getOptionsFromLocal = () => {
  if (typeof window !== "undefined") {
    const data = localStorage.getItem(OPTIONS_KEY);
    const options = data ? JSON.parse(data) : { ...DEFAULT_OPTIONS };

    // Initialize http_port_number if not exists
    if (!options.http_port_number) {
      options.http_port_number = Math.floor(Math.random() * 1000) + 8000; // Random port between 8000-9000
      localStorage.setItem(OPTIONS_KEY, JSON.stringify(options));
    }

    return options;
  }
  return DEFAULT_OPTIONS;
};

export const setOptionsToLocal = (options: IOptions) => {
  if (typeof window !== "undefined") {
    localStorage.setItem(OPTIONS_KEY, JSON.stringify(options));
  }
};

export const getTrulienceSettingsFromLocal = () => {
  if (typeof window !== "undefined") {
    const data = localStorage.getItem(TRULIENCE_SETTINGS_KEY);
    if (data) {
      return JSON.parse(data);
    }
  }
  return DEFAULT_TRULIENCE_OPTIONS;
};

export const setTrulienceSettingsToLocal = (settings: ITrulienceSettings) => {
  if (typeof window !== "undefined") {
    localStorage.setItem(TRULIENCE_SETTINGS_KEY, JSON.stringify(settings));
  }
};
