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
}

export const apiStartService = async (config: StartRequestConfig): Promise<any> => {
    // The request will be rewritten by middleware to send to AGENT_SERVER_URL
    const url = `/api/agents/start`;
    const { channel, userId, graphName, language, voiceType } = config;
    const data = {
        request_id: genUUID(),
        channel_name: channel,
        user_uid: userId,
        graph_name: graphName,
        language,
        voice_type: voiceType
    };

    let resp: any = await axios.post(url, data);
    resp = (resp.data) || {};
    return resp;
};

export const apiStopService = async (channel: string) => {
    // The request will be rewritten by middleware to send to AGENT_SERVER_URL
    const url = `/api/agents/stop`;
    const data = {
        request_id: genUUID(),
        channel_name: channel
    };

    let resp: any = await axios.post(url, data);
    resp = (resp.data) || {};
    return resp;
};

export const apiSaveMemory = async (channel: string) => {
    // The request will be rewritten by middleware to send to AGENT_SERVER_URL
    const url = `/api/agents/save_memory`;
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
    // The request will be rewritten by middleware to send to AGENT_SERVER_URL
    const url = `/api/agents/ping`;
    const data = {
        request_id: genUUID(),
        channel_name: channel
    };
    let resp: any = await axios.post(url, data);
    resp = (resp.data) || {};
    return resp;
};
