"use client";

import * as React from "react";
import { toast } from "sonner";
import {
  apiPing,
  apiStartService,
  apiStopService,
  EMobileActiveTab,
  MOBILE_ACTIVE_TAB_MAP,
  type StartRequestConfig,
  useAppDispatch,
  useAppSelector,
} from "@/common";
import { LoadingButton } from "@/components/Button/LoadingButton";
import SettingsDialog, {
  cozeSettingsFormSchema,
  difySettingsFormSchema,
  isCozeGraph,
  isDifyGraph,
  oceanbaseSettingsFormSchema,
} from "@/components/Dialog/Settings";
import { Tabs, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { cn } from "@/lib/utils";
import {
  setAgentConnected,
  setGlobalSettingsDialog,
  setMobileActiveTab,
} from "@/store/reducers/global";
import type { IOceanBaseSettings } from "@/types";

let intervalId: NodeJS.Timeout | null = null;
const isSuccessCode = (code: unknown) => code === 0 || code === "0";

export default function Action(props: { className?: string }) {
  const { className } = props;
  const dispatch = useAppDispatch();
  const agentConnected = useAppSelector((state) => state.global.agentConnected);
  const channel = useAppSelector((state) => state.global.options.channel);
  const userId = useAppSelector((state) => state.global.options.userId);
  const language = useAppSelector((state) => state.global.language);
  const voiceType = useAppSelector((state) => state.global.voiceType);
  const graphName = useAppSelector((state) => state.global.graphName);
  const agentSettings = useAppSelector((state) => state.global.agentSettings);
  const cozeSettings = useAppSelector((state) => state.global.cozeSettings);
  const difySettings = useAppSelector((state) => state.global.difySettings);
  const oceanbaseSettings = useAppSelector(
    (state) => state.global.oceanbaseSettings
  );
  const mobileActiveTab = useAppSelector(
    (state) => state.global.mobileActiveTab
  );

  const [loading, setLoading] = React.useState(false);

  const checkAgentConnected = async () => {
    const res: any = await apiPing(channel);
    if (isSuccessCode(res?.code)) {
      dispatch(setAgentConnected(true));
    }
  };

  React.useEffect(() => {
    if (channel) {
      checkAgentConnected();
    }
  }, [channel, checkAgentConnected]);

  const onClickConnect = async () => {
    if (loading) {
      return;
    }
    setLoading(true);
    try {
      if (agentConnected) {
        // handle disconnect
        const res = await apiStopService(channel);
        const { code, msg } = res || {};
        if (!isSuccessCode(code)) {
          toast.error(`code:${code},msg:${msg}`);
          throw new Error(msg || "Failed to disconnect agent");
        }
        dispatch(setAgentConnected(false));
        toast.success("Agent disconnected");
        stopPing();
      } else {
        // handle connect
        // prepare start service payload
        const startServicePayload: StartRequestConfig = {
          channel,
          userId,
          graphName,
          language,
          voiceType,
          greeting: agentSettings.greeting,
          prompt: agentSettings.prompt,
        };
        // check graph ---
        if (isCozeGraph(graphName)) {
          // check coze settings
          const cozeSettingsResult =
            cozeSettingsFormSchema.safeParse(cozeSettings);
          if (!cozeSettingsResult.success) {
            dispatch(
              setGlobalSettingsDialog({
                open: true,
                tab: "coze",
              })
            );
            throw new Error(
              "Invalid Coze settings. Please check your settings."
            );
          }
          startServicePayload.coze_token = cozeSettingsResult.data.token;
          startServicePayload.coze_bot_id = cozeSettingsResult.data.bot_id;
          startServicePayload.coze_base_url = cozeSettingsResult.data.base_url;
        } else if (isDifyGraph(graphName)) {
          const difySettingsResult =
            difySettingsFormSchema.safeParse(difySettings);
          if (!difySettingsResult.success) {
            dispatch(
              setGlobalSettingsDialog({
                open: true,
                tab: "dify",
              })
            );
            throw new Error(
              "Invalid Dify settings. Please check your settings."
            );
          }
          startServicePayload.dify_api_key = difySettingsResult.data.api_key;
          startServicePayload.dify_base_url = difySettingsResult.data.base_url;
        } else if (graphName.includes("oceanbase")) {
          const oceanBaseSettingsResult =
            oceanbaseSettingsFormSchema.safeParse(oceanbaseSettings);
          if (!oceanBaseSettingsResult.success) {
            dispatch(
              setGlobalSettingsDialog({
                open: true,
                tab: "oceanbase",
              })
            );
            throw new Error(
              "Invalid OceanBase settings. Please check your settings."
            );
          }
          const settings: IOceanBaseSettings = {
            api_key: oceanBaseSettingsResult.data.api_key,
            base_url: oceanBaseSettingsResult.data.base_url,
            db_name: oceanBaseSettingsResult.data.db_name,
            collection_id: oceanBaseSettingsResult.data.collection_id,
          };
          startServicePayload.oceanbase_settings = settings;
        }
        // common -- start service
        const res = await apiStartService(startServicePayload);
        const { code, msg } = res || {};
        if (!isSuccessCode(code)) {
          if (code === "10001") {
            toast.error(
              "The number of users experiencing the program simultaneously has exceeded the limit. Please try again later."
            );
          } else {
            toast.error(`code:${code},msg:${msg}`);
          }
          throw new Error(msg);
        }
        dispatch(setAgentConnected(true));
        toast.success("Agent connected");
        startPing();
      }
    } catch (error) {
      console.error(error);
      toast.error("Failed to connect/disconnect agent", {
        description: (error as Error)?.message,
      });
    } finally {
      setLoading(false);
    }
  };

  const startPing = () => {
    if (intervalId) {
      stopPing();
    }
    intervalId = setInterval(() => {
      apiPing(channel);
    }, 3000);
  };

  const stopPing = () => {
    if (intervalId) {
      clearInterval(intervalId);
      intervalId = null;
    }
  };

  const onChangeMobileActiveTab = (tab: string) => {
    dispatch(setMobileActiveTab(tab as EMobileActiveTab));
  };

  return (
    <>
      {/* Action Bar */}
      <div
        className={cn(
          "mx-2 mt-2 flex items-center justify-between rounded-t-lg bg-[#181a1d] p-2 md:m-2 md:rounded-lg",
          className
        )}
      >
        {/* -- Description Part */}
        <div className="hidden md:block">
          <span className="font-bold text-sm">Description</span>
          <span className="ml-2 text-muted-foreground text-xs">
            Multi-Purpose Voice Assistant Agent Example Powered by TEN
          </span>
        </div>

        <Tabs
          defaultValue={mobileActiveTab}
          className="w-[400px] md:hidden"
          onValueChange={onChangeMobileActiveTab}
        >
          <TabsList>
            {Object.values(EMobileActiveTab).map((tab) => (
              <TabsTrigger key={tab} value={tab} className="w-24 text-sm">
                {MOBILE_ACTIVE_TAB_MAP[tab]}
              </TabsTrigger>
            ))}
          </TabsList>
        </Tabs>

        {/* -- Action Button */}
        <div className="ml-auto flex items-center gap-2">
          <SettingsDialog />
          <LoadingButton
            onClick={onClickConnect}
            variant={!agentConnected ? "default" : "destructive"}
            size="sm"
            className="w-fit min-w-24"
            loading={loading}
            svgProps={{ className: "h-4 w-4 text-muted-foreground" }}
          >
            {loading
              ? "Connecting"
              : !agentConnected
                ? "Connect"
                : "Disconnect"}
          </LoadingButton>
        </div>
      </div>
    </>
  );
}
