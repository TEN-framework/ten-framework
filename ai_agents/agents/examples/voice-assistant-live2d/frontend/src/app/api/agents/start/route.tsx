import { NextRequest, NextResponse } from 'next/server';
import axios from 'axios';
import { getGraphProperties } from '@/lib/graphProperties';

/**
 * Handles the POST request to start an agent.
 *
 * @param request - The NextRequest object representing the incoming request.
 * @returns A NextResponse object representing the response to be sent back to the client.
 */
export async function POST(request: NextRequest) {
    try {
        const { AGENT_SERVER_URL } = process.env;

        // Check if environment variables are available
        if (!AGENT_SERVER_URL) {
            throw "Environment variables are not available";
        }

        const body = await request.json();
        const {
            request_id,
            channel_name,
            user_uid,
            graph_name,
            language,
            voice_type,
            character_id,
            greeting,
            prompt,
            properties,
        } = body;

        const computedProperties = getGraphProperties(
            graph_name,
            language,
            voice_type,
            character_id,
            prompt,
            greeting,
        );

        const mergedProperties = {
            ...computedProperties,
            ...(properties || {}),
        };

        console.log("[API] Start merged properties", {
            greeting:
                (mergedProperties as any)?.main_control?.greeting ??
                (mergedProperties as any)?.llm?.greeting,
            voice_id:
                (mergedProperties as any)?.tts?.params?.voice_setting?.voice_id,
        });

        const payload: Record<string, unknown> = {
            request_id,
            channel_name,
            user_uid,
            graph_name,
        };

        if (Object.keys(mergedProperties).length > 0) {
            payload.properties = mergedProperties;
        }

        // Send a POST request to start the agent
        const response = await axios.post(`${AGENT_SERVER_URL}/start`, payload);

        const responseData = response.data;

        return NextResponse.json(responseData, { status: response.status });
    } catch (error: any) {
        console.error("[API] Start agent failed:", error);

        if (axios.isAxiosError(error)) {
            const status = error.response?.status || 500;
            const data = error.response?.data || {
                code: "1",
                msg: error.message || "Internal Server Error",
                data: null
            };
            return NextResponse.json(data, { status });
        }

        if (error instanceof Response) {
            const errorData = await error.json();
            return NextResponse.json(errorData, { status: error.status });
        } else {
            return NextResponse.json({
                code: "1",
                data: null,
                msg: typeof error === 'string' ? error : (error.message || "Internal Server Error")
            }, { status: 500 });
        }
    }
}
