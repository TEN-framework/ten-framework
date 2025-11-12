'use client';

import React, { forwardRef, useCallback, useEffect, useImperativeHandle, useMemo, useRef, useState } from 'react';
// Import PIXI setup first to ensure global availability
import PIXI from '@/lib/pixi-setup';
import { MotionSync } from 'live2d-motionsync/stream';
// IRemoteAudioTrack will be imported dynamically
import { cn } from '@/lib/utils';

declare global {
    interface Window {
        tenLive2d?: Record<string, {
            setExpression: (name?: string) => Promise<boolean>;
            setRandomExpression: () => Promise<boolean>;
            playMotion: (name: string, options?: { priority?: number }) => Promise<boolean>;
            getExpressions: () => ExpressionConfig[];
            getMotions: () => MotionConfig[];
        }>;
    }
}

export interface ExpressionConfig {
    name: string;
    label?: string;
    default?: boolean;
    onSpeaking?: boolean;
}

export interface MotionConfig {
    name: string;
    label?: string;
    group: string;
    index: number;
    autoPlay?: boolean;
    loop?: boolean;
    onSpeakingStart?: boolean;
    priority?: number;
}

export type MouthConfig =
    | {
        type: 'open';
        openId: string;
        formId?: string;
    }
    | {
        type: 'corners';
        upId: string;
        downId: string;
        formId?: string;
    };

export interface Live2DHandle {
    setExpression: (name?: string) => Promise<boolean>;
    setRandomExpression: () => Promise<boolean>;
    playMotion: (name: string, options?: { priority?: number }) => Promise<boolean>;
    getAvailableExpressions: () => ExpressionConfig[];
    getAvailableMotions: () => MotionConfig[];
}

interface Live2DCharacterProps {
    modelPath: string;
    audioTrack?: any;
    className?: string;
    mouthConfig?: MouthConfig;
    expressions?: ExpressionConfig[];
    motions?: MotionConfig[];
    onModelLoaded?: () => void;
    onModelError?: (error: Error) => void;
}

const Live2DCharacter = forwardRef<Live2DHandle, Live2DCharacterProps>(function Live2DCharacter({
    modelPath,
    audioTrack,
    className,
    mouthConfig,
    expressions,
    motions,
    onModelLoaded,
    onModelError
}, ref) {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const motionSyncRef = useRef<MotionSync | null>(null);
    const appRef = useRef<any>(null);
    const [isModelLoaded, setIsModelLoaded] = useState(false);
    const [isClient, setIsClient] = useState(false);
    const audioElementRef = useRef<HTMLAudioElement | null>(null);
    const [motionSyncEnabled, setMotionSyncEnabled] = useState(true); // Re-enabled for lip sync
    const isDisconnectingRef = useRef(false);
    const live2dModelRef = useRef<any>(null);
    const audioContextRef = useRef<AudioContext | null>(null);
    const analyserRef = useRef<AnalyserNode | null>(null);
    const dataArrayRef = useRef<Uint8Array<ArrayBuffer> | null>(null);
    const lipSyncSourceRef = useRef<MediaStreamAudioSourceNode | null>(null);
    const lipSyncAnimationRef = useRef<number | null>(null);
    const mouthOpenValueRef = useRef(0);
    const expressionMapRef = useRef<Map<string, ExpressionConfig>>(new Map());
    const expressionsListRef = useRef<ExpressionConfig[]>([]);
    const defaultExpressionRef = useRef<string | undefined>(undefined);
    const motionMapRef = useRef<Map<string, MotionConfig>>(new Map());
    const motionsListRef = useRef<MotionConfig[]>([]);
    const idleMotionNameRef = useRef<string | undefined>(undefined);
    type ParamConfig = {
        id: string;
        index: number;
        min: number;
        max: number;
        defaultValue: number;
    };
    const mouthParamConfigRef = useRef<{
        open?: ParamConfig;
        form?: ParamConfig;
        up?: ParamConfig;
        down?: ParamConfig;
    }>({});
    const motionPriority = useMemo(() => {
        const runtimePriority = (PIXI as any)?.live2d?.MotionPriority;
        return runtimePriority ?? { NONE: 0, IDLE: 1, NORMAL: 2, FORCE: 3 };
    }, []);

    useEffect(() => {
        const registry = new Map<string, ExpressionConfig>();
        (expressions ?? []).forEach((expression) => {
            registry.set(expression.name, expression);
        });
        expressionMapRef.current = registry;
        expressionsListRef.current = expressions ?? [];
        defaultExpressionRef.current = (expressions ?? []).find((expression) => expression.default)?.name;
    }, [expressions]);

    useEffect(() => {
        const registry = new Map<string, MotionConfig>();
        (motions ?? []).forEach((motion) => {
            registry.set(motion.name, motion);
        });
        motionMapRef.current = registry;
        motionsListRef.current = motions ?? [];
        idleMotionNameRef.current = (motions ?? []).find((motion) => motion.autoPlay)?.name;
    }, [motions]);

    const applyDefaultExpression = useCallback(async (modelInstance: any) => {
        const defaultExpression = defaultExpressionRef.current;
        if (!modelInstance) {
            return false;
        }
        if (!defaultExpression) {
            const manager = modelInstance.internalModel?.motionManager?.expressionManager;
            manager?.resetExpression();
            return true;
        }
        try {
            await modelInstance.expression(defaultExpression);
            return true;
        } catch (error) {
            console.warn('[Live2DCharacter] Unable to apply default expression', defaultExpression, error);
            return false;
        }
    }, []);

    const ensureIdleMotion = useCallback(async (modelInstance: any) => {
        const idleName = idleMotionNameRef.current;
        if (!modelInstance || !idleName) {
            return false;
        }
        const idleConfig = motionMapRef.current.get(idleName);
        if (!idleConfig) {
            return false;
        }
        try {
            return await modelInstance.motion(
                idleConfig.group,
                idleConfig.index,
                idleConfig.priority ?? motionPriority.IDLE ?? 1
            );
        } catch (error) {
            console.warn('[Live2DCharacter] Unable to start idle motion', idleName, error);
            return false;
        }
    }, [motionPriority]);

    const setExpression = useCallback(async (name?: string) => {
        const model = live2dModelRef.current;
        if (!model) {
            return false;
        }
        const manager = model.internalModel?.motionManager?.expressionManager;
        if (!manager) {
            return false;
        }
        if (!name) {
            manager.resetExpression();
            const defaultName = defaultExpressionRef.current;
            if (defaultName) {
                try {
                    await model.expression(defaultName);
                    return true;
                } catch (error) {
                    console.warn('[Live2DCharacter] Unable to fallback to default expression', defaultName, error);
                    return false;
                }
            }
            return true;
        }
        if (!expressionMapRef.current.has(name)) {
            console.warn(`[Live2DCharacter] Expression "${name}" is not registered for ${modelPath}`);
        }
        try {
            return await model.expression(name);
        } catch (error) {
            console.warn('[Live2DCharacter] Unable to apply expression', name, error);
            return false;
        }
    }, [modelPath]);

    const setRandomExpression = useCallback(async () => {
        const model = live2dModelRef.current;
        if (!model) {
            return false;
        }
        try {
            return await model.expression();
        } catch (error) {
            console.warn('[Live2DCharacter] Unable to apply random expression', error);
            return false;
        }
    }, []);

    const playMotion = useCallback(async (name: string, options?: { priority?: number }) => {
        const model = live2dModelRef.current;
        if (!model) {
            return false;
        }
        const motionConfig = motionMapRef.current.get(name);
        if (!motionConfig) {
            console.warn(`[Live2DCharacter] Motion "${name}" is not registered for ${modelPath}`);
            return false;
        }
        try {
            const priority = options?.priority ?? motionConfig.priority ?? motionPriority.NORMAL ?? 2;
            return await model.motion(motionConfig.group, motionConfig.index, priority);
        } catch (error) {
            console.warn('[Live2DCharacter] Unable to play motion', name, error);
            return false;
        }
    }, [modelPath, motionPriority]);

    useImperativeHandle(ref, () => ({
        setExpression,
        setRandomExpression,
        playMotion,
        getAvailableExpressions: () => expressionsListRef.current,
        getAvailableMotions: () => motionsListRef.current
    }), [playMotion, setExpression, setRandomExpression]);

    useEffect(() => {
        if (typeof window === 'undefined') {
            return;
        }
        const api = {
            setExpression,
            setRandomExpression,
            playMotion,
            getExpressions: () => expressionsListRef.current,
            getMotions: () => motionsListRef.current
        };
        window.tenLive2d ??= {};
        window.tenLive2d[modelPath] = api;
        return () => {
            if (window.tenLive2d) {
                delete window.tenLive2d[modelPath];
            }
        };
    }, [modelPath, playMotion, setExpression, setRandomExpression]);

    const buildParamConfig = (coreModel: any, parameterId?: string | null): ParamConfig | undefined => {
        if (!coreModel || !parameterId || typeof coreModel.getParameterIndex !== "function") {
            return undefined;
        }
        let index: number;
        try {
            index = coreModel.getParameterIndex(parameterId);
        } catch {
            return undefined;
        }
        if (typeof index !== "number" || index < 0) {
            return undefined;
        }
        const min = typeof coreModel.getParameterMinimumValue === "function" ? coreModel.getParameterMinimumValue(index) : -1;
        const max = typeof coreModel.getParameterMaximumValue === "function" ? coreModel.getParameterMaximumValue(index) : 1;
        const defaultValue =
            typeof coreModel.getParameterDefaultValue === "function" ? coreModel.getParameterDefaultValue(index) : 0;
        return {
            id: parameterId,
            index,
            min,
            max,
            defaultValue
        };
    };

    const resolveMouthParameters = (coreModel: any, provided?: MouthConfig) => {
        if (!coreModel) {
            mouthParamConfigRef.current = {};
            return;
        }

        let parameterIds: string[] = [];
        try {
            const ids = typeof coreModel.getParameterIds === "function" ? coreModel.getParameterIds() : [];
            if (Array.isArray(ids)) {
                parameterIds = ids;
            } else if (ids && typeof (ids as ArrayLike<string>).length === "number") {
                parameterIds = Array.from(ids as ArrayLike<string>);
            }
        } catch {
            parameterIds = [];
        }

        if (!parameterIds.length && typeof coreModel.getParameterCount === "function" && typeof coreModel.getParameterId === "function") {
            const count = coreModel.getParameterCount();
            for (let i = 0; i < count; i++) {
                try {
                    const id = coreModel.getParameterId(i);
                    if (typeof id === "string") {
                        parameterIds.push(id);
                    }
                } catch {
                    // ignore this index and continue
                }
            }
        }

        const findId = (...predicates: ((id: string) => boolean)[]) => {
            for (const predicate of predicates) {
                const found = parameterIds.find(predicate);
                if (found) {
                    return found;
                }
            }
            return undefined;
        };

        let openId: string | undefined;
        let formId: string | undefined;
        let upId: string | undefined;
        let downId: string | undefined;

        if (provided?.type === 'open') {
            openId = provided.openId;
            formId = provided.formId;
        } else if (provided?.type === 'corners') {
            upId = provided.upId;
            downId = provided.downId;
            formId = provided.formId;
        }

        const ensureId = (current: string | undefined, ...predicates: ((id: string) => boolean)[]) =>
            current ?? findId(...predicates);

        openId = ensureId(
            openId,
            (id) => /parammouthopeny/i.test(id),
            (id) => /parammouthopen/i.test(id),
            (id) => /mouthopen/i.test(id)
        );
        formId = ensureId(
            formId,
            (id) => /parammouthform/i.test(id),
            (id) => /mouthform/i.test(id),
            (id) => /mouthshape/i.test(id)
        );
        upId = ensureId(
            upId,
            (id) => /parammouthup/i.test(id),
            (id) => /mouthupper/i.test(id)
        );
        downId = ensureId(
            downId,
            (id) => /parammouthdown/i.test(id),
            (id) => /mouthlower/i.test(id)
        );

        mouthParamConfigRef.current = {
            open: buildParamConfig(coreModel, openId),
            form: buildParamConfig(coreModel, formId),
            up: buildParamConfig(coreModel, upId),
            down: buildParamConfig(coreModel, downId)
        };

        if (process.env.NODE_ENV !== "production") {
            console.log("[Live2DCharacter] Resolved mouth params", {
                openId,
                formId,
                upId,
                downId,
                config: mouthParamConfigRef.current
            });
        }
    };

    const applyMouthParameter = (config: ParamConfig | undefined, target: number) => {
        const coreModel = live2dModelRef.current?.internalModel?.coreModel;
        if (!config || !coreModel) {
            return;
        }
        const clamped = Math.min(config.max, Math.max(config.min, target));
        try {
            coreModel.setParameterValueById(config.id, clamped);
        } catch (error) {
            console.warn("[Live2DCharacter] Failed to set parameter value for", config.id, error);
        }
    };

    const stopFallbackLipSync = () => {
        if (lipSyncAnimationRef.current !== null) {
            cancelAnimationFrame(lipSyncAnimationRef.current);
            lipSyncAnimationRef.current = null;
        }
        if (lipSyncSourceRef.current) {
            try {
                lipSyncSourceRef.current.disconnect();
            } catch { }
            lipSyncSourceRef.current = null;
        }
        analyserRef.current = null;
        dataArrayRef.current = null;
        mouthOpenValueRef.current = 0;
        const config = mouthParamConfigRef.current;
        applyMouthParameter(config.open, config.open?.defaultValue ?? 0);
        applyMouthParameter(config.form, config.form?.defaultValue ?? 0);
        applyMouthParameter(config.up, config.up?.defaultValue ?? 0);
        applyMouthParameter(config.down, config.down?.defaultValue ?? 0);
    };

    const ensureAudioContext = async () => {
        if (typeof window === "undefined") return null;
        if (!audioContextRef.current) {
            const AudioCtx = (window.AudioContext || (window as any).webkitAudioContext) as typeof AudioContext | undefined;
            if (!AudioCtx) {
                console.warn("[Live2DCharacter] Web Audio API is not available; fallback lip sync disabled.");
                return null;
            }
            audioContextRef.current = new AudioCtx();
        }
        if (audioContextRef.current.state === "suspended") {
            try {
                await audioContextRef.current.resume();
            } catch (error) {
                console.warn("[Live2DCharacter] Unable to resume AudioContext:", error);
            }
        }
        return audioContextRef.current;
    };

    const startFallbackLipSync = async (stream: MediaStream) => {
        const audioContext = await ensureAudioContext();
        if (!audioContext || !live2dModelRef.current) {
            return;
        }

        stopFallbackLipSync();

        try {
            const source = audioContext.createMediaStreamSource(stream);
            const analyser = audioContext.createAnalyser();
            analyser.fftSize = 1024;
            analyser.smoothingTimeConstant = 0.6;

            source.connect(analyser);

            analyserRef.current = analyser;
            lipSyncSourceRef.current = source;
            dataArrayRef.current = new Uint8Array(analyser.frequencyBinCount);

            const animateMouth = () => {
                if (!analyserRef.current || !dataArrayRef.current || !live2dModelRef.current) {
                    lipSyncAnimationRef.current = null;
                    return;
                }

                analyserRef.current.getByteTimeDomainData(dataArrayRef.current);
                let sum = 0;
                for (let i = 0; i < dataArrayRef.current.length; i++) {
                    const value = (dataArrayRef.current[i] - 128) / 128;
                    sum += Math.abs(value);
                }
                const average = sum / dataArrayRef.current.length;
                const target = Math.min(1, Math.max(0, (average - 0.02) * 4));
                const current = mouthOpenValueRef.current;
                const smoothed = current + (target - current) * 0.35;
                mouthOpenValueRef.current = smoothed;

                try {
                    const config = mouthParamConfigRef.current;
                    if (config.open) {
                        const range = config.open.max - config.open.min || 1;
                        const targetOpen = config.open.min + smoothed * range;
                        applyMouthParameter(config.open, targetOpen);
                        if (config.form) {
                            const formRange = config.form.max - config.form.min || 1;
                            const targetForm = config.form.min + smoothed * formRange;
                            applyMouthParameter(config.form, targetForm);
                        }
                    } else {
                        if (config.up) {
                            const positiveSpan = Math.max(config.up.max - config.up.defaultValue, 0);
                            const negativeSpan = Math.max(config.up.defaultValue - config.up.min, 0);
                            let targetUp = config.up.defaultValue;
                            if (positiveSpan >= negativeSpan) {
                                targetUp = config.up.defaultValue + smoothed * (positiveSpan || 1);
                            } else {
                                targetUp = config.up.defaultValue - smoothed * (negativeSpan || 1);
                            }
                            applyMouthParameter(config.up, targetUp);
                        }
                        if (config.down) {
                            const positiveSpan = Math.max(config.down.max - config.down.defaultValue, 0);
                            const negativeSpan = Math.max(config.down.defaultValue - config.down.min, 0);
                            let targetDown = config.down.defaultValue;
                            if (positiveSpan >= negativeSpan) {
                                targetDown = config.down.defaultValue + smoothed * (positiveSpan || 1);
                            } else {
                                targetDown = config.down.defaultValue - smoothed * (negativeSpan || 1);
                            }
                            applyMouthParameter(config.down, targetDown);
                        }
                        if (config.form) {
                            const formRange = config.form.max - config.form.min || 1;
                            const targetForm = config.form.min + smoothed * formRange;
                            applyMouthParameter(config.form, targetForm);
                        }
                    }
                } catch (error) {
                    console.warn("[Live2DCharacter] Failed to update mouth parameters:", error);
                }

                lipSyncAnimationRef.current = requestAnimationFrame(animateMouth);
            };

            lipSyncAnimationRef.current = requestAnimationFrame(animateMouth);
        } catch (error) {
            console.warn("[Live2DCharacter] Fallback lip sync setup failed:", error);
        }
    };

    // Ensure component only renders on client side
    useEffect(() => {
        setIsClient(true);

        // Add global error handler for MotionSync errors
        const handleGlobalError = (event: ErrorEvent) => {
            if (event.message && event.message.includes('addLast')) {
                console.error('[Live2DCharacter] MotionSync error caught globally:', event);
                setMotionSyncEnabled(false);
                // Prevent the error from propagating
                event.preventDefault();
                return false;
            }
        };

        const handleUnhandledRejection = (event: PromiseRejectionEvent) => {
            if (event.reason && event.reason.toString().includes('addLast')) {
                console.error('[Live2DCharacter] MotionSync promise rejection caught:', event.reason);
                setMotionSyncEnabled(false);
                event.preventDefault();
            }
        };

        window.addEventListener('error', handleGlobalError);
        window.addEventListener('unhandledrejection', handleUnhandledRejection);

        return () => {
            window.removeEventListener('error', handleGlobalError);
            window.removeEventListener('unhandledrejection', handleUnhandledRejection);
        };
    }, []);

    useEffect(() => {
        // Ensure only runs on client side
        if (typeof window === "undefined") return;

        // Ensure we have a valid model path
        if (!modelPath) {
            console.log('[Live2DCharacter] No model path provided, skipping initialization');
            return;
        }

        // Wait for Live2D core library to load
        const waitForLive2DCore = () => {
            return new Promise<void>((resolve) => {
                if (typeof window !== "undefined" && (window as any).Live2DCubismCore) {
                    resolve();
                    return;
                }

                const checkInterval = setInterval(() => {
                    if (typeof window !== "undefined" && (window as any).Live2DCubismCore) {
                        clearInterval(checkInterval);
                        resolve();
                    }
                }, 100);

                // Timeout handling
                setTimeout(() => {
                    clearInterval(checkInterval);
                    console.error("Live2D Cubism Core failed to load within timeout");
                }, 10000);
            });
        };

        const initLive2D = async () => {
            try {
                console.log('[Live2DCharacter] Initializing with model path:', modelPath);
                await waitForLive2DCore();

                // Small delay to prevent rapid re-initialization
                await new Promise(resolve => setTimeout(resolve, 100));

                stopFallbackLipSync();

                // Clean up any existing PIXI application
                if (appRef.current) {
                    console.log('[Live2DCharacter] Cleaning up existing PIXI application');
                    try {
                        // Stop the application first
                        appRef.current.stop();

                        // Destroy with aggressive cleanup
                        appRef.current.destroy(true, {
                            children: true,
                            texture: true,
                            baseTexture: true
                        });

                        // Clear the canvas context
                        if (canvasRef.current) {
                            const ctx = canvasRef.current.getContext('2d');
                            if (ctx) {
                                ctx.clearRect(0, 0, canvasRef.current.width, canvasRef.current.height);
                            }
                        }
                    } catch (destroyError) {
                        console.warn('[Live2DCharacter] Error destroying PIXI app:', destroyError);
                    }
                    appRef.current = null;
                }

                // Ensure canvas is clean and ready
                if (canvasRef.current) {
                    const canvas = canvasRef.current;
                    const ctx = canvas.getContext('webgl2') || canvas.getContext('webgl') || canvas.getContext('2d');
                    if (ctx) {
                        // Clear any existing context
                        if ('clear' in ctx) {
                            ctx.clear(ctx.COLOR_BUFFER_BIT || 0x00004000);
                        }
                    }
                }

                // Create new PIXI application with Canvas renderer to avoid WebGL shader issues
                console.log('[Live2DCharacter] Creating PIXI Application with Canvas renderer...');
                const app = new PIXI.Application({
                    view: canvasRef.current!,
                    autoStart: true,
                    resizeTo: canvasRef.current?.parentElement || window,
                    backgroundColor: 0x000000,
                    backgroundAlpha: 0,
                    forceCanvas: true, // Force Canvas renderer to avoid WebGL shader issues
                    antialias: false, // Disable antialiasing to reduce GPU usage
                    powerPreference: 'low-power', // Use integrated GPU to avoid crashes
                });
                console.log('[Live2DCharacter] PIXI Application created successfully:', app);

                appRef.current = app;

                // Wait a moment for PIXI application to fully initialize
                await new Promise(resolve => setTimeout(resolve, 100));

                // Validate PIXI application is properly initialized
                console.log('[Live2DCharacter] Validating PIXI Application...');
                console.log('[Live2DCharacter] App:', app);
                console.log('[Live2DCharacter] App.stage:', app?.stage);
                if (!app || !app.stage) {
                    throw new Error('PIXI Application or stage is not properly initialized');
                }
                console.log('[Live2DCharacter] PIXI Application validation passed');

                // Load Live2D with proper PIXI setup
                const { Live2DModel } = await import('@/lib/live2d-loader').then(loader => loader.loadLive2DModel());
                let model: any;

                console.log('[Live2DCharacter] Loading model from:', modelPath);
                model = await Live2DModel.from(modelPath);

                // Validate model is loaded before adding to stage
                if (!model) {
                    throw new Error('Failed to load Live2D model');
                }

                app.stage.addChild(model);
                live2dModelRef.current = model;
                resolveMouthParameters(model?.internalModel?.coreModel, mouthConfig);
                await applyDefaultExpression(model);
                await ensureIdleMotion(model);

                // Adjust model size and position
                const parent = canvasRef.current?.parentElement;
                if (parent) {
                    model.scale.set(parent.clientHeight / model.height);
                    model.x = (parent.clientWidth - model.width) / 2;
                }

                // Initialize MotionSync if available and enabled
                if (motionSyncEnabled) {
                    try {
                        const motionSyncUrl = modelPath.replace('.model3.json', '.motionsync3.json');
                        console.log('[Live2DCharacter] Attempting to load MotionSync from:', motionSyncUrl);

                        // Check if MotionSync file exists by making a HEAD request
                        const response = await fetch(motionSyncUrl, { method: 'HEAD' });
                        if (response.ok) {
                            // Wait a bit for the model to be fully initialized
                            await new Promise(resolve => setTimeout(resolve, 1000));

                            // Validate that the model and internalModel are properly initialized
                            if (model && model.internalModel && model.internalModel.coreModel) {
                                console.log("[Live2DCharacter] Creating MotionSync instance...");
                                const motionSync = new MotionSync(model.internalModel);

                                console.log("[Live2DCharacter] Loading MotionSync from URL...");
                                await motionSync.loadMotionSyncFromUrl(motionSyncUrl);

                                motionSyncRef.current = motionSync;
                                console.log("[Live2DCharacter] MotionSync loaded successfully");
                            } else {
                                console.warn("[Live2DCharacter] Model internal structure not ready for MotionSync");
                                motionSyncRef.current = null;
                            }
                        } else {
                            console.log("[Live2DCharacter] MotionSync file not found, skipping MotionSync initialization");
                            motionSyncRef.current = null;
                        }
                    } catch (motionSyncError) {
                        console.error("[Live2DCharacter] MotionSync initialization failed:", motionSyncError);
                        setMotionSyncEnabled(false);
                        motionSyncRef.current = null;
                    }
                } else {
                    console.log("[Live2DCharacter] MotionSync disabled due to previous errors");
                    motionSyncRef.current = null;
                }

                setIsModelLoaded(true);
                onModelLoaded?.();
                console.log("Live2D Model is ready.");

            } catch (error) {
                console.error("Failed to initialize Live2D:", error);
                onModelError?.(error as Error);
            }
        };

        initLive2D();

        // Cleanup function
        return () => {
            console.log('[Live2DCharacter] Cleaning up Live2D resources');
            if (appRef.current) {
                appRef.current.destroy(false, true);
                appRef.current = null;
            }
            motionSyncRef.current = null;
            live2dModelRef.current = null;
            mouthParamConfigRef.current = {};
            setIsModelLoaded(false);
        };
    }, [modelPath, mouthConfig, expressions, motions, onModelLoaded, onModelError, motionSyncEnabled]);

    // Effect for handling audioTrack from Agora
    useEffect(() => {
        const motionSync = motionSyncRef.current;
        // Ensure model is loaded (MotionSync is optional)
        if (!isModelLoaded) return;

        if (audioTrack && audioTrack.getMediaStreamTrack) {
            console.log("[Live2DCharacter] Received audioTrack, creating MediaStream.");
            isDisconnectingRef.current = false; // Reset disconnect flag

            // Create MediaStream from Agora audio track
            const stream = new MediaStream([audioTrack.getMediaStreamTrack()]);

            // Pass stream to motionSync for playback and lip sync (if available)
            if (motionSync && motionSyncEnabled && !isDisconnectingRef.current) {
                try {
                    // Wrap in a promise to catch any async errors
                    Promise.resolve().then(() => {
                        if (!isDisconnectingRef.current && motionSync) {
                            motionSync.play(stream);
                            console.log("[Live2DCharacter] MotionSync audio playback started");
                            stopFallbackLipSync();
                        }
                    }).catch((playError) => {
                        console.error("[Live2DCharacter] MotionSync play promise error:", playError);
                        setMotionSyncEnabled(false);
                        stopFallbackLipSync();
                        startFallbackLipSync(stream);
                    });
                } catch (motionSyncPlayError) {
                    console.error("[Live2DCharacter] MotionSync play error:", motionSyncPlayError);
                    setMotionSyncEnabled(false);
                    stopFallbackLipSync();
                    startFallbackLipSync(stream);
                    // Continue without MotionSync if it fails
                }
            } else {
                console.log("[Live2DCharacter] MotionSync not available or disabled, audio will play without lip sync");
                startFallbackLipSync(stream);
            }

            // Also create and play hidden <audio> element to ensure actual sound
            try {
                if (!audioElementRef.current) {
                    const audio = document.createElement("audio");
                    audio.autoplay = true;
                    // playsInline needed in iOS/Safari to avoid fullscreen
                    (audio as any).playsInline = true;
                    audio.muted = false;
                    audio.volume = 1.0;
                    audio.style.display = "none";
                    document.body.appendChild(audio);
                    audioElementRef.current = audio;
                }
                const audioEl = audioElementRef.current!;
                audioEl.srcObject = stream;
                const playPromise = audioEl.play();
                if (playPromise && typeof playPromise.then === "function") {
                    playPromise.catch((err: unknown) => {
                        console.warn("[Live2DCharacter] Autoplay blocked, waiting for user gesture.", err);
                    });
                }
            } catch (err) {
                console.error("[Live2DCharacter] Failed to play audio:", err);
            }

            // Reset lip sync when audio track ends
            audioTrack.getMediaStreamTrack().onended = () => {
                console.log("[Live2DCharacter] Audio track ended.");
                isDisconnectingRef.current = true; // Set disconnect flag

                if (motionSync && !isDisconnectingRef.current) {
                    try {
                        motionSync.reset();
                    } catch (resetError) {
                        console.error("[Live2DCharacter] MotionSync reset error:", resetError);
                    }
                }
                if (audioElementRef.current) {
                    try {
                        audioElementRef.current.pause();
                        audioElementRef.current.srcObject = null;
                        audioElementRef.current.remove();
                    } catch { }
                    audioElementRef.current = null;
                }
            };
        } else {
            // If no audioTrack (including null during disconnect), reset lip sync
            console.log("[Live2DCharacter] No audioTrack, resetting MotionSync.");
            isDisconnectingRef.current = true; // Set disconnect flag

            stopFallbackLipSync();

            if (motionSync && !isDisconnectingRef.current) {
                try {
                    // Add a small delay to ensure any ongoing audio processing completes
                    setTimeout(() => {
                        try {
                            if (!isDisconnectingRef.current && motionSync) {
                                motionSync.reset();
                            }
                        } catch (resetError) {
                            console.error("[Live2DCharacter] MotionSync reset error:", resetError);
                        }
                    }, 100);
                } catch (resetError) {
                    console.error("[Live2DCharacter] MotionSync reset error:", resetError);
                }
            }
            if (audioElementRef.current) {
                try {
                    audioElementRef.current.pause();
                    audioElementRef.current.srcObject = null;
                    audioElementRef.current.remove();
                } catch { }
                audioElementRef.current = null;
            }
        }

        return () => {
            // Clean up and reset when component unmounts or audioTrack changes
            isDisconnectingRef.current = true; // Set disconnect flag

            stopFallbackLipSync();

            if (motionSync && !isDisconnectingRef.current) {
                try {
                    motionSync.reset();
                } catch (resetError) {
                    console.error("[Live2DCharacter] MotionSync reset error:", resetError);
                }
            }
            if (audioElementRef.current) {
                try {
                    audioElementRef.current.pause();
                    audioElementRef.current.srcObject = null;
                    audioElementRef.current.remove();
                } catch { }
                audioElementRef.current = null;
            }
        };
    }, [audioTrack, isModelLoaded]);

    // Component unmount cleanup
    useEffect(() => {
        return () => {
            console.log('[Live2DCharacter] Component unmounting, cleaning up all resources');

            // Clean up MotionSync first with delay
            isDisconnectingRef.current = true; // Set disconnect flag

            stopFallbackLipSync();

            if (motionSyncRef.current) {
                try {
                    setTimeout(() => {
                        try {
                            if (!isDisconnectingRef.current && motionSyncRef.current) {
                                motionSyncRef.current.reset();
                            }
                        } catch (resetError) {
                            console.error("[Live2DCharacter] MotionSync reset error during unmount:", resetError);
                        }
                    }, 50);
                } catch (error) {
                    console.error("[Live2DCharacter] MotionSync cleanup error:", error);
                }
                motionSyncRef.current = null;
            }

            // Clean up audio element
            if (audioElementRef.current) {
                try {
                    audioElementRef.current.pause();
                    audioElementRef.current.srcObject = null;
                    audioElementRef.current.remove();
                } catch { }
                audioElementRef.current = null;
            }

            // Clean up PIXI app last
            if (appRef.current) {
                try {
                    // Stop the application first
                    appRef.current.stop();

                    // Destroy with aggressive cleanup
                    appRef.current.destroy(true, {
                        children: true,
                        texture: true,
                        baseTexture: true
                    });

                    // Clear the canvas context
                    if (canvasRef.current) {
                        const ctx = canvasRef.current.getContext('2d');
                        if (ctx) {
                            ctx.clearRect(0, 0, canvasRef.current.width, canvasRef.current.height);
                        }
                    }
                } catch (destroyError) {
                    console.error("[Live2DCharacter] PIXI app destroy error:", destroyError);
                }
                appRef.current = null;
            }
        };
    }, []);

    // Show loading state during SSR or before client hydration
    if (!isClient) {
        return (
            <div className={cn("relative h-full w-full bg-gradient-to-b from-blue-50 to-blue-100 dark:from-gray-900 dark:to-gray-800", className)}>
                <div className="absolute inset-0 flex items-center justify-center">
                    <div className="text-center">
                        <div className="animate-spin rounded-full h-12 w-12 border-b-2 border-primary mx-auto mb-4"></div>
                        <p className="text-muted-foreground">Loading Live2D Model...</p>
                    </div>
                </div>
            </div>
        );
    }

    return (
        <div className={cn("relative h-full w-full", className)}>
            <canvas
                ref={canvasRef}
                key={`live2d-canvas-${modelPath}`} // Force canvas recreation when model changes
                style={{ display: 'block' }}
            />
            {!isModelLoaded && (
                <div className="absolute inset-0 z-10 flex items-center justify-center bg-black bg-opacity-50 text-white">
                    <div className="text-center">
                        <div className="animate-spin rounded-full h-12 w-12 border-b-2 border-white mx-auto mb-4"></div>
                        <p>Loading Live2D Model...</p>
                    </div>
                </div>
            )}
        </div>
    );
});

export default Live2DCharacter;
