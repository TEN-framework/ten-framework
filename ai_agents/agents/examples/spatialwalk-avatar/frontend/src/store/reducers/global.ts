import {
  createAsyncThunk,
  createSlice,
  type PayloadAction,
} from "@reduxjs/toolkit";
import {
  COLOR_LIST,
  DEFAULT_OPTIONS,
  DEFAULT_SPATIALWALK_OPTIONS,
  EMobileActiveTab,
  isEditModeOn,
} from "@/common/constant";
import type { AddonDef, Graph } from "@/common/graph";
import { useAppSelector } from "@/common/hooks";
import {
  apiFetchGraphDetails,
  apiFetchGraphs,
  apiFetchInstalledAddons,
  apiLoadApp,
  apiReloadPackage,
  apiSaveProperty,
  apiUpdateGraph,
} from "@/common/request";
import {
  setOptionsToLocal,
  setSpatialwalkSettingsToLocal,
} from "@/common/storage";
import type {
  FestivalEffectName,
  IChatItem,
  IFestivalEffect,
  IFortuneModal,
  IOptions,
  ISpatialwalkSettings,
  Language,
  VoiceType,
} from "@/types";

export interface InitialState {
  options: IOptions;
  roomConnected: boolean;
  agentConnected: boolean;
  rtmConnected: boolean;
  themeColor: string;
  language: Language;
  voiceType: VoiceType;
  chatItems: IChatItem[];
  selectedGraphId: string;
  graphList: Graph[];
  graphMap: Record<string, Graph>;
  addonModules: AddonDef.Module[]; // addon modules
  mobileActiveTab: EMobileActiveTab;
  spatialwalkSettings: ISpatialwalkSettings;
  festivalEffect: IFestivalEffect | null;
  fortuneModal: IFortuneModal | null;
}

const getInitialState = (): InitialState => {
  return {
    options: DEFAULT_OPTIONS,
    themeColor: COLOR_LIST[0].active,
    roomConnected: false,
    agentConnected: false,
    rtmConnected: false,
    language: "en-US",
    voiceType: "male",
    chatItems: [],
    selectedGraphId: "",
    graphList: [],
    graphMap: {},
    addonModules: [],
    mobileActiveTab: EMobileActiveTab.AGENT,
    spatialwalkSettings: DEFAULT_SPATIALWALK_OPTIONS,
    festivalEffect: null,
    fortuneModal: null,
  };
};

export const globalSlice = createSlice({
  name: "global",
  initialState: getInitialState(),
  reducers: {
    setOptions: (state, action: PayloadAction<Partial<IOptions>>) => {
      state.options = { ...state.options, ...action.payload };
      setOptionsToLocal(state.options);
    },
    setSpatialwalkSettings: (
      state,
      action: PayloadAction<ISpatialwalkSettings>
    ) => {
      state.spatialwalkSettings = {
        ...state.spatialwalkSettings,
        ...action.payload,
        avatarDesktopLargeWindow: true,
      };
      setSpatialwalkSettingsToLocal(state.spatialwalkSettings);
    },
    setThemeColor: (state, action: PayloadAction<string>) => {
      state.themeColor = action.payload;
      document.documentElement.style.setProperty(
        "--theme-color",
        action.payload
      );
    },
    setRoomConnected: (state, action: PayloadAction<boolean>) => {
      state.roomConnected = action.payload;
    },
    setRtmConnected: (state, action: PayloadAction<boolean>) => {
      state.rtmConnected = action.payload;
    },
    addChatItem: (state, action: PayloadAction<IChatItem>) => {
      const { userId, text, isFinal, type, time } = action.payload;
      const LastFinalIndex = state.chatItems.findLastIndex((el) => {
        return el.userId == userId && el.isFinal;
      });
      const LastNonFinalIndex = state.chatItems.findLastIndex((el) => {
        return el.userId == userId && !el.isFinal;
      });
      const LastFinalItem = state.chatItems[LastFinalIndex];
      const LastNonFinalItem = state.chatItems[LastNonFinalIndex];
      if (LastFinalItem) {
        // has last final Item
        if (time <= LastFinalItem.time) {
          // discard
          console.log(
            "[test] addChatItem, time < last final item, discard!:",
            text,
            isFinal,
            type
          );
          return;
        } else {
          if (LastNonFinalItem) {
            console.log(
              "[test] addChatItem, update last item(none final):",
              text,
              isFinal,
              type
            );
            state.chatItems[LastNonFinalIndex] = action.payload;
          } else {
            console.log(
              "[test] addChatItem, add new item:",
              text,
              isFinal,
              type
            );
            state.chatItems.push(action.payload);
          }
        }
      } else {
        // no last final Item
        if (LastNonFinalItem) {
          console.log(
            "[test] addChatItem, update last item(none final):",
            text,
            isFinal,
            type
          );
          state.chatItems[LastNonFinalIndex] = action.payload;
        } else {
          console.log("[test] addChatItem, add new item:", text, isFinal, type);
          state.chatItems.push(action.payload);
        }
      }
      state.chatItems.sort((a, b) => a.time - b.time);
    },
    setAgentConnected: (state, action: PayloadAction<boolean>) => {
      state.agentConnected = action.payload;
    },
    setLanguage: (state, action: PayloadAction<Language>) => {
      state.language = action.payload;
    },
    setSelectedGraphId: (state, action: PayloadAction<string>) => {
      state.selectedGraphId = action.payload;
    },
    setGraphList: (state, action: PayloadAction<Graph[]>) => {
      state.graphList = action.payload;
    },
    setVoiceType: (state, action: PayloadAction<VoiceType>) => {
      state.voiceType = action.payload;
    },
    setMobileActiveTab: (state, action: PayloadAction<EMobileActiveTab>) => {
      state.mobileActiveTab = action.payload;
    },
    triggerFestivalEffect: (
      state,
      action: PayloadAction<{ name: FestivalEffectName }>
    ) => {
      state.festivalEffect = {
        name: action.payload.name,
        active: true,
        nonce: Date.now(),
      };
    },
    clearFestivalEffect: (state) => {
      state.festivalEffect = null;
    },
    showFortuneModal: (
      state,
      action: PayloadAction<{ imageId: string }>
    ) => {
      state.fortuneModal = {
        open: true,
        imageId: action.payload.imageId,
      };
    },
    hideFortuneModal: (state) => {
      state.fortuneModal = null;
    },
    reset: (state) => {
      Object.assign(state, getInitialState());
      document.documentElement.style.setProperty(
        "--theme-color",
        COLOR_LIST[0].active
      );
    },
    setGraph: (state, action: PayloadAction<Graph>) => {
      const graphMap = JSON.parse(JSON.stringify(state.graphMap));
      graphMap[action.payload.graph_id] = action.payload;
      state.graphMap = graphMap;
    },
    setAddonModules: (state, action: PayloadAction<Record<string, any>[]>) => {
      state.addonModules = JSON.parse(JSON.stringify(action.payload));
    },
  },
});

// Initialize graph data
let initializeGraphData: any;
// Fetch graph details
let fetchGraphDetails: any;

// if (isEditModeOn) {
//   // only for development, below requests depend on dev-server
//   initializeGraphData = createAsyncThunk(
//     "global/initializeGraphData",
//     async (_, { dispatch }) => {
//       try {
//         await apiReloadPackage();
//       } catch (error) {
//         console.warn("Error reloading package:", error);
//       }
//       await apiLoadApp();
//       const [fetchedGraphs, modules] = await Promise.all([
//         apiFetchGraphs(),
//         apiFetchInstalledAddons(),
//       ]);
//       dispatch(setGraphList(fetchedGraphs.map((graph) => graph)));
//       dispatch(setAddonModules(modules));
//     }
//   );
//   fetchGraphDetails = createAsyncThunk(
//     "global/fetchGraphDetails",
//     async (graph: Graph, { dispatch }) => {
//       const updatedGraph = await apiFetchGraphDetails(graph);
//       dispatch(setGraph(updatedGraph));
//     }
//   );
// } else {
initializeGraphData = createAsyncThunk(
  "global/initializeGraphData",
  async (_, { dispatch }) => {
    const fetchedGraphs = await apiFetchGraphs();
    dispatch(setGraphList(fetchedGraphs.map((graph) => graph)));
  }
);
fetchGraphDetails = createAsyncThunk(
  "global/fetchGraphDetails",
  async (graphId: string, { dispatch }) => {
    // Do nothing in production
    return;
  }
);
// }

// Update a graph
export const updateGraph = createAsyncThunk(
  "global/updateGraph",
  async (
    { graph, updates }: { graph: Graph; updates: Partial<Graph> },
    { dispatch, rejectWithValue }
  ) => {
    try {
      await apiUpdateGraph(graph.graph_id, updates);
      // await apiSaveProperty();
      const updatedGraph = await apiFetchGraphDetails(graph);
      dispatch(setGraph(updatedGraph));
      return updatedGraph; // Optionally return the updated graph
    } catch (error: any) {
      // Handle error gracefully
      console.error("Error updating graph:", error);
      return rejectWithValue(error.response?.data || error.message);
    }
  }
);

export const {
  reset,
  setOptions,
  setRoomConnected,
  setAgentConnected,
  setRtmConnected,
  setVoiceType,
  addChatItem,
  setThemeColor,
  setLanguage,
  setSelectedGraphId,
  setGraphList,
  setMobileActiveTab,
  setGraph,
  setAddonModules,
  setSpatialwalkSettings,
  triggerFestivalEffect,
  clearFestivalEffect,
  showFortuneModal,
  hideFortuneModal,
} = globalSlice.actions;

export { initializeGraphData, fetchGraphDetails };

export default globalSlice.reducer;
