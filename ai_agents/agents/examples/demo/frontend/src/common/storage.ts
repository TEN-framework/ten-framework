import type {
  IAgentSettings,
  ICozeSettings,
  IDifySettings,
  IOceanBaseSettings,
  IOptions,
} from "@/types";
import {
  AGENT_SETTINGS_KEY,
  COZE_SETTINGS_KEY,
  DEFAULT_AGENT_SETTINGS,
  DEFAULT_COZE_SETTINGS,
  DEFAULT_DIFY_SETTINGS,
  DEFAULT_OCEAN_BASE_SETTINGS,
  DEFAULT_OPTIONS,
  DIFY_SETTINGS_KEY,
  OCEANBASE_SETTINGS_KEY,
  OPTIONS_KEY,
} from "./constant";

export const getOptionsFromLocal = (): {
  options: IOptions;
  settings: IAgentSettings;
  cozeSettings: ICozeSettings;
  difySettings: IDifySettings;
  oceanbaseSettings: IOceanBaseSettings;
} => {
  const data = {
    options: DEFAULT_OPTIONS,
    settings: DEFAULT_AGENT_SETTINGS,
    cozeSettings: DEFAULT_COZE_SETTINGS,
    oceanbaseSettings: DEFAULT_OCEAN_BASE_SETTINGS,
    difySettings: DEFAULT_DIFY_SETTINGS,
  };
  if (typeof window !== "undefined") {
    const options = localStorage.getItem(OPTIONS_KEY);
    if (options) {
      data.options = JSON.parse(options);
    }
    const settings = localStorage.getItem(AGENT_SETTINGS_KEY);
    if (settings) {
      data.settings = JSON.parse(settings);
    }
    const cozeSettings = localStorage.getItem(COZE_SETTINGS_KEY);
    if (cozeSettings) {
      data.cozeSettings = JSON.parse(cozeSettings);
    }
    const difySettings = localStorage.getItem(DIFY_SETTINGS_KEY);
    if (difySettings) {
      data.difySettings = JSON.parse(difySettings);
    }
    const oceanbaseSettings = localStorage.getItem(OCEANBASE_SETTINGS_KEY);
    if (oceanbaseSettings) {
      data.oceanbaseSettings = JSON.parse(oceanbaseSettings);
    }
  }
  return data;
};

export const setOptionsToLocal = (options: IOptions) => {
  if (typeof window !== "undefined") {
    localStorage.setItem(OPTIONS_KEY, JSON.stringify(options));
  }
};

export const setAgentSettingsToLocal = (settings: IAgentSettings) => {
  if (typeof window !== "undefined") {
    localStorage.setItem(AGENT_SETTINGS_KEY, JSON.stringify(settings));
  }
};

export const setCozeSettingsToLocal = (settings: ICozeSettings) => {
  if (typeof window !== "undefined") {
    localStorage.setItem(COZE_SETTINGS_KEY, JSON.stringify(settings));
  }
};

export const setDifySettingsToLocal = (settings: IDifySettings) => {
  if (typeof window !== "undefined") {
    localStorage.setItem(DIFY_SETTINGS_KEY, JSON.stringify(settings));
  }
};

export const setOceanBaseSettingsToLocal = (settings: IOceanBaseSettings) => {
  if (typeof window !== "undefined") {
    localStorage.setItem(OCEANBASE_SETTINGS_KEY, JSON.stringify(settings));
  }
};

export const resetSettingsByKeys = (keys: string | string[]) => {
  if (typeof window !== "undefined") {
    if (Array.isArray(keys)) {
      keys.forEach((key) => {
        localStorage.removeItem(key);
      });
    } else {
      localStorage.removeItem(keys);
    }
  }
};

export const resetCozeSettings = () => {
  resetSettingsByKeys(COZE_SETTINGS_KEY);
};

export const resetDifySettings = () => {
  resetSettingsByKeys(DIFY_SETTINGS_KEY);
};

export const resetOceanBaseSettings = () => {
  resetSettingsByKeys(OCEANBASE_SETTINGS_KEY);
};
