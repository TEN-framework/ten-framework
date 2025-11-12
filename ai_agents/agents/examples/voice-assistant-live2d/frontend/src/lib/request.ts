import axios from 'axios';

// Generate a simple UUID-like string
function genUUID(): string {
    return Math.random().toString(36).substring(2, 15) + Math.random().toString(36).substring(2, 15);
}

interface StartRequestConfig {
    channel: string;
    userId: number;
    graphName: string;
    language: string;
    voiceType: "male" | "female";
    properties?: Record<string, unknown>;
}

export const apiStartService = async (config: StartRequestConfig): Promise<any> => {
    const url = `/api/agents/start`;
    const { channel, userId, graphName, language, voiceType, properties } = config;
    const data: Record<string, unknown> = {
        request_id: genUUID(),
        channel_name: channel,
        user_uid: userId,
        graph_name: graphName,
        language,
        voice_type: voiceType
    };
    if (properties) {
        data.properties = properties;
    }

    let resp: any = await axios.post(url, data);
    resp = (resp.data) || {};
    return resp;
};

export const apiStopService = async (channel: string) => {
    const url = `/api/agents/stop`;
    const data = {
        request_id: genUUID(),
        channel_name: channel
    };

    let resp: any = await axios.post(url, data);
    resp = (resp.data) || {};
    return resp;
};

// ping/pong
export const apiPing = async (channel: string) => {
    // the request will be rewrite at middleware.tsx to send to $AGENT_SERVER_URL
    const url = `/api/agents/ping`;
    const data = {
        request_id: genUUID(),
        channel_name: channel
    };
    let resp: any = await axios.post(url, data);
    resp = (resp.data) || {};
    return resp;
};
