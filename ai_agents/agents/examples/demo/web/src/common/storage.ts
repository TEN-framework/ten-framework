import { IAgentSettings, IOptions, ICozeSettings, IDifySettings, IOceanBaseSettings } from "@/types"
import {
  OPTIONS_KEY,
  DEFAULT_OPTIONS,
  AGENT_SETTINGS_KEY,
  DEFAULT_AGENT_SETTINGS,
  COZE_SETTINGS_KEY,
  DEFAULT_COZE_SETTINGS,
  DIFY_SETTINGS_KEY,
  DEFAULT_DIFY_SETTINGS,
  DEFAULT_OCEAN_BASE_SETTINGS,
  OCEANBASE_SETTINGS_KEY,
} from "./constant"

export const getOptionsFromLocal = (): {
  options: IOptions
  settings: IAgentSettings
  cozeSettings: ICozeSettings
  difySettings: IDifySettings
  oceanbaseSettings: IOceanBaseSettings
} => {
  let data = {
    options: DEFAULT_OPTIONS,
    settings: DEFAULT_AGENT_SETTINGS,
    cozeSettings: DEFAULT_COZE_SETTINGS,
    oceanbaseSettings: DEFAULT_OCEAN_BASE_SETTINGS,
    difySettings: DEFAULT_DIFY_SETTINGS,
  }
  if (typeof window !== "undefined") {
    const options = localStorage.getItem(OPTIONS_KEY)
    if (options) {
      data.options = JSON.parse(options)
    }
    const settings = localStorage.getItem(AGENT_SETTINGS_KEY)
    if (settings) {
      data.settings = JSON.parse(settings)
    }
    const cozeSettings = localStorage.getItem(COZE_SETTINGS_KEY)
    if (cozeSettings) {
      data.cozeSettings = JSON.parse(cozeSettings)
    }
    const difySettings = localStorage.getItem(DIFY_SETTINGS_KEY)
    if (difySettings) {
      data.difySettings = JSON.parse(difySettings)
    }
    const oceanbaseSettings = localStorage.getItem(OCEANBASE_SETTINGS_KEY)
    if (oceanbaseSettings) {
      data.oceanbaseSettings = JSON.parse(oceanbaseSettings)
    }
  }
  return data
}

export const setOptionsToLocal = (options: IOptions) => {
  if (typeof window !== "undefined") {
    localStorage.setItem(OPTIONS_KEY, JSON.stringify(options))
  }
}

export const setAgentSettingsToLocal = (settings: IAgentSettings) => {
  if (typeof window !== "undefined") {
    localStorage.setItem(AGENT_SETTINGS_KEY, JSON.stringify(settings))
  }
}

export const setCozeSettingsToLocal = (settings: ICozeSettings) => {
  if (typeof window !== "undefined") {
    localStorage.setItem(COZE_SETTINGS_KEY, JSON.stringify(settings))
  }
}

export const setDifySettingsToLocal = (settings: IDifySettings) => {
  if (typeof window !== "undefined") {
    localStorage.setItem(DIFY_SETTINGS_KEY, JSON.stringify(settings))
  }
}

export const setOceanBaseSettingsToLocal = (settings: IOceanBaseSettings) => {
  if (typeof window !== "undefined") {
    localStorage.setItem(OCEANBASE_SETTINGS_KEY, JSON.stringify(settings))
  }
}

export const resetSettingsByKeys = (keys: string | string[]) => {
  if (typeof window !== "undefined") {
    if (Array.isArray(keys)) {
      keys.forEach((key) => {
        localStorage.removeItem(key)
      })
    } else {
      localStorage.removeItem(keys)
    }
  }
}

export const resetCozeSettings = () => {
  resetSettingsByKeys(COZE_SETTINGS_KEY)
}

export const resetDifySettings = () => {
  resetSettingsByKeys(DIFY_SETTINGS_KEY)
}

export const resetOceanBaseSettings = () => {
  resetSettingsByKeys(OCEANBASE_SETTINGS_KEY)
}
