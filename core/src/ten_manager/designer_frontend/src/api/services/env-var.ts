import {
    getQueryHookCache,
    makeAPIRequest,
} from "@/api/services/utils";
import {
    ENDPOINT_METHOD,
    ENV_VAR_AGORA_APP_CERT,
    ENV_VAR_AGORA_APP_ID,
} from "@/api/endpoints/constant";
import { ENDPOINT_ENV_VAR } from "../endpoints/env-var";
import React from "react";
import { IRTCEnvVar } from "@/types/env-var";

export const getEnvVar = async (name: string) => {
    const template = ENDPOINT_ENV_VAR.getEnvVar[ENDPOINT_METHOD.POST];
    const req = makeAPIRequest(template, {
        body: { name },
    });
    const res = await req;
    return template.responseSchema.parse(res);
};

export const useRTCEnvVar = () => {
    const template = ENDPOINT_ENV_VAR.getEnvVar[ENDPOINT_METHOD.POST];
    const queryHookCache = getQueryHookCache();
    const cacheKey = `env-var-rtc`;

    const [data, setData] = React.useState<IRTCEnvVar | null>(() => {
        const [cachedData, cachedDataIsExpired] =
            queryHookCache.get<IRTCEnvVar>(cacheKey);
        if (!cachedData || cachedDataIsExpired) {
            return null;
        }
        return cachedData;
    });
    const [error, setError] = React.useState<Error | null>(null);
    const [isLoading, setIsLoading] = React.useState<boolean>(false);

    const fetchData = React.useCallback(async () => {
        setIsLoading(true);
        try {
            const reqAppId = makeAPIRequest(template, {
                body: template.requestSchema.parse({
                    name: ENV_VAR_AGORA_APP_ID,
                }),
            });
            const reqAppCert = makeAPIRequest(template, {
                body: template.requestSchema.parse({
                    name: ENV_VAR_AGORA_APP_CERT,
                }),
            });
            const [resAppId, resAppCert] = await Promise.all([
                reqAppId, reqAppCert,
            ]);
            const parsedAppId = template.responseSchema.parse(resAppId);
            const parsedAppCert = template.responseSchema
                .parse(resAppCert);
            
            if (!parsedAppId.value) {
                throw new Error("AGORA_APP_ID is not set");
            }

            const parsedData = {
                appId: parsedAppId.value,
                appCert: parsedAppCert.value,
            };
            setData(parsedData);
            queryHookCache.set(cacheKey, parsedData);
        } catch (err) {
            setError(err as Error);
        } finally {
            setIsLoading(false);
        }
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [cacheKey]);

    React.useEffect(() => {
        fetchData();
    }, [fetchData]);

    return {
        value: data,
        error,
        isLoading,
        mutate: fetchData,
    };
};