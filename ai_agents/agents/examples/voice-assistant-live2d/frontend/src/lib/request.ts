import axios, { AxiosError } from 'axios';
import { getGraphProperties } from './graphProperties';

// Generate a simple UUID-like string
function genUUID(): string {
    return Math.random().toString(36).substring(2, 15) + Math.random().toString(36).substring(2, 15);
}

// Retry with exponential backoff
async function retryWithBackoff<T>(
    fn: () => Promise<T>,
    maxRetries: number = 3,
    initialDelay: number = 1000
): Promise<T> {
    let lastError: Error | undefined;

    for (let attempt = 0; attempt < maxRetries; attempt++) {
        try {
            return await fn();
        } catch (error) {
            lastError = error as Error;
            if (attempt < maxRetries - 1) {
                const delay = initialDelay * Math.pow(2, attempt);
                console.log(`Request failed (attempt ${attempt + 1}/${maxRetries}), retrying in ${delay}ms...`, error);
                await new Promise(resolve => setTimeout(resolve, delay));
            }
        }
    }

    throw lastError;
}

// Configure axios with timeout
const API_TIMEOUT = 10000; // 10 seconds

interface StartRequestConfig {
    channel: string;
    userId: number;
    graphName: string;
    language: string;
    voiceType: "male" | "female";
    characterId?: string;
    greeting?: string;
    prompt?: string;
    properties?: Record<string, unknown>;
}

export const apiStartService = async (config: StartRequestConfig): Promise<any> => {
    const base = (process.env.NEXT_PUBLIC_API_BASE_URL || '').replace(/\/$/, '');
    const primary = base ? `${base}/start` : `/api/agents/start`;
    const { channel, userId, graphName, language, voiceType, greeting, prompt, properties, characterId } = config;
    const computedProperties = getGraphProperties(
        graphName,
        language,
        voiceType,
        characterId,
        prompt,
        greeting,
    );
    const data: Record<string, unknown> = {
        request_id: genUUID(),
        channel_name: channel,
        user_uid: userId,
        graph_name: graphName,
        language,
        voice_type: voiceType,
    };
    if (greeting) {
        data.greeting = greeting;
    }
    if (prompt) {
        data.prompt = prompt;
    }
    if (characterId) {
        data.character_id = characterId;
    }
    const mergedProperties = {
        ...computedProperties,
        ...(properties || {}),
    };
    if (Object.keys(mergedProperties).length > 0) {
        data.properties = mergedProperties;
    }

    // Debug helper to verify what we actually send to the backend.
    // Enable with NEXT_PUBLIC_DEBUG_AGENT_START=1
    if (process.env.NEXT_PUBLIC_DEBUG_AGENT_START === "1") {
        // eslint-disable-next-line no-console
        console.log("[API] Start payload (debug)", {
            primary,
            graphName,
            channel,
            userId,
            voiceType,
            characterId,
            greeting,
            prompt,
            computedProperties,
            mergedProperties,
            resolvedVoiceId:
                (mergedProperties as any)?.tts?.params?.voice_setting?.voice_id,
            resolvedGreeting:
                (mergedProperties as any)?.main_control?.greeting ??
                (mergedProperties as any)?.llm?.greeting,
        });
    }

    return retryWithBackoff(async () => {
        try {
            console.log(`[API] Starting service with graph: ${graphName}, channel: ${channel}`);
            const resp = await axios.post(primary, data, { timeout: API_TIMEOUT });
            console.log('[API] Start service success:', resp.data);
            return resp.data || {};
        } catch (error) {
            const axiosError = error as AxiosError;
            console.error('[API] Primary endpoint failed:', primary, axiosError.message);

            const fallback = `/api/agents/start`;
            console.log('[API] Trying fallback endpoint:', fallback);

            try {
                const resp = await axios.post(fallback, data, { timeout: API_TIMEOUT });
                console.log('[API] Fallback success:', resp.data);
                return resp.data || {};
            } catch (fallbackError) {
                const fallbackAxiosError = fallbackError as AxiosError;
                console.error('[API] Fallback endpoint failed:', fallback, fallbackAxiosError.message);
                throw new Error(`Failed to start service: ${fallbackAxiosError.message}`);
            }
        }
    }, 2, 1000); // 2 retries with 1 second initial delay
};

export const apiStopService = async (channel: string) => {
    const base = (process.env.NEXT_PUBLIC_API_BASE_URL || '').replace(/\/$/, '');
    const primary = base ? `${base}/stop` : `/api/agents/stop`;
    const data = {
        request_id: genUUID(),
        channel_name: channel
    };

    return retryWithBackoff(async () => {
        try {
            console.log(`[API] Stopping service for channel: ${channel}`);
            const resp = await axios.post(primary, data, { timeout: API_TIMEOUT });
            console.log('[API] Stop service success:', resp.data);
            return resp.data || {};
        } catch (error) {
            const axiosError = error as AxiosError;
            console.error('[API] Primary endpoint failed:', primary, axiosError.message);

            const fallback = `/api/agents/stop`;
            console.log('[API] Trying fallback endpoint:', fallback);

            try {
                const resp = await axios.post(fallback, data, { timeout: API_TIMEOUT });
                console.log('[API] Fallback success:', resp.data);
                return resp.data || {};
            } catch (fallbackError) {
                const fallbackAxiosError = fallbackError as AxiosError;
                console.error('[API] Fallback endpoint failed:', fallback, fallbackAxiosError.message);
                throw new Error(`Failed to stop service: ${fallbackAxiosError.message}`);
            }
        }
    }, 2, 1000);
};

// ping/pong
export const apiPing = async (channel: string) => {
    const base = (process.env.NEXT_PUBLIC_API_BASE_URL || '').replace(/\/$/, '');
    const primary = base ? `${base}/ping` : `/api/agents/ping`;
    const data = {
        request_id: genUUID(),
        channel_name: channel
    };

    // Note: Ping doesn't use retry logic to avoid blocking during connection issues
    try {
        const resp = await axios.post(primary, data, { timeout: API_TIMEOUT });
        return resp.data || {};
    } catch (error) {
        const axiosError = error as AxiosError;
        console.warn('[API] Ping primary endpoint failed:', primary, axiosError.message);

        const fallback = `/api/agents/ping`;
        try {
            const resp = await axios.post(fallback, data, { timeout: API_TIMEOUT });
            return resp.data || {};
        } catch (fallbackError) {
            const fallbackAxiosError = fallbackError as AxiosError;
            console.warn('[API] Ping fallback endpoint failed:', fallback, fallbackAxiosError.message);
            throw new Error(`Failed to ping service: ${fallbackAxiosError.message}`);
        }
    }
};
