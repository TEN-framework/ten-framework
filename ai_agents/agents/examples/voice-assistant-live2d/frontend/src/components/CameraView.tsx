'use client';

import React, { useEffect, useRef, useState } from 'react';
import AgoraRTC, { ICameraVideoTrack } from 'agora-rtc-sdk-ng';
import { cn } from '@/lib/utils';

// Declare global type for VirtualBackgroundExtension
declare global {
    interface Window {
        VirtualBackgroundExtension: any;
    }
}

interface CameraViewProps {
    className?: string;
    backgroundColor?: string; // hex color for virtual background
    enableVirtualBackground?: boolean;
}

export default function CameraView({
    className,
    backgroundColor = '#ffffff',
    enableVirtualBackground = true
}: CameraViewProps) {
    const videoContainerRef = useRef<HTMLDivElement>(null);
    const [videoTrack, setVideoTrack] = useState<ICameraVideoTrack | null>(null);
    const [processor, setProcessor] = useState<any>(null);
    const [isReady, setIsReady] = useState(false);
    const [error, setError] = useState<string | null>(null);
    const [isVBEnabled, setIsVBEnabled] = useState(false);
    const extensionRef = useRef<any>(null);

    // Initialize camera
    useEffect(() => {
        let mounted = true;
        let track: ICameraVideoTrack | null = null;

        const initCamera = async () => {
            try {
                console.log('[CameraView] Initializing camera...');

                // Create camera track
                track = await AgoraRTC.createCameraVideoTrack({
                    optimizationMode: 'detail',
                });

                if (!mounted) {
                    track.close();
                    return;
                }

                setVideoTrack(track);

                // Play video in container
                if (videoContainerRef.current) {
                    track.play(videoContainerRef.current, { fit: 'cover' });
                    console.log('[CameraView] Camera track playing');
                }

                setIsReady(true);
            } catch (err) {
                console.error('[CameraView] Failed to initialize camera:', err);
                setError('Failed to access camera. Please check permissions.');
            }
        };

        initCamera();

        return () => {
            mounted = false;
            if (track) {
                track.stop();
                track.close();
            }
        };
    }, []);

    // Initialize virtual background
    useEffect(() => {
        if (!videoTrack || !enableVirtualBackground || !isReady) return;

        let mounted = true;
        let currentProcessor: any = null;

        const initVirtualBackground = async () => {
            try {
                console.log('[CameraView] Initializing virtual background...');

                // Wait for VirtualBackgroundExtension to be loaded
                if (typeof window === 'undefined' || !window.VirtualBackgroundExtension) {
                    console.warn('[CameraView] VirtualBackgroundExtension not loaded yet, retrying...');
                    setTimeout(initVirtualBackground, 500);
                    return;
                }

                // Create extension if not exists
                if (!extensionRef.current) {
                    extensionRef.current = new window.VirtualBackgroundExtension();
                    AgoraRTC.registerExtensions([extensionRef.current]);
                    console.log('[CameraView] VirtualBackgroundExtension registered');
                }

                // Create processor
                currentProcessor = extensionRef.current.createProcessor();

                // Listen for performance warnings
                currentProcessor.eventBus.on('PERFORMANCE_WARNING', () => {
                    console.warn('[CameraView] Virtual background performance warning');
                });

                // Initialize processor with wasm files
                await currentProcessor.init('/agora-extension-virtual-background/wasms');
                console.log('[CameraView] Processor initialized');

                if (!mounted) {
                    currentProcessor.disable();
                    return;
                }

                // Pipe processor to video track
                videoTrack.pipe(currentProcessor).pipe(videoTrack.processorDestination);
                console.log('[CameraView] Processor piped to video track');

                // Enable processor
                await currentProcessor.enable();
                console.log('[CameraView] Virtual background enabled');

                // Set color background
                await currentProcessor.setOptions({
                    type: 'color',
                    color: backgroundColor
                });
                console.log('[CameraView] Background color set to:', backgroundColor);

                setProcessor(currentProcessor);
                setIsVBEnabled(true);

            } catch (err) {
                console.error('[CameraView] Failed to initialize virtual background:', err);
                setError('Failed to initialize virtual background');
            }
        };

        initVirtualBackground();

        return () => {
            mounted = false;
            if (currentProcessor) {
                try {
                    currentProcessor.disable();
                    currentProcessor.unpipe();
                } catch (e) {
                    console.error('[CameraView] Error cleaning up processor:', e);
                }
            }
        };
    }, [videoTrack, enableVirtualBackground, isReady, backgroundColor]);

    // Update background color when it changes
    useEffect(() => {
        if (processor && isVBEnabled && enableVirtualBackground) {
            const updateColor = async () => {
                try {
                    await processor.setOptions({
                        type: 'color',
                        color: backgroundColor
                    });
                    console.log('[CameraView] Background color updated to:', backgroundColor);
                } catch (err) {
                    console.error('[CameraView] Failed to update background color:', err);
                }
            };
            updateColor();
        }
    }, [backgroundColor, processor, isVBEnabled, enableVirtualBackground]);

    return (
        <div className={cn("relative w-full h-full overflow-hidden rounded-lg bg-black", className)}>
            <div
                ref={videoContainerRef}
                className="w-full h-full"
                style={{ display: 'block' }}
            />

            {/* Loading state */}
            {!isReady && !error && (
                <div className="absolute inset-0 flex items-center justify-center bg-black bg-opacity-70">
                    <div className="text-center text-white">
                        <div className="animate-spin rounded-full h-12 w-12 border-b-2 border-white mx-auto mb-4"></div>
                        <p>Initializing camera...</p>
                    </div>
                </div>
            )}

            {/* Error state */}
            {error && (
                <div className="absolute inset-0 flex items-center justify-center bg-red-900 bg-opacity-70">
                    <div className="text-center text-white">
                        <p className="font-semibold mb-2">Error</p>
                        <p className="text-sm">{error}</p>
                    </div>
                </div>
            )}

            {/* Status indicator */}
            {isReady && !error && (
                <div className="absolute top-2 left-2 flex items-center gap-2 bg-black bg-opacity-50 px-3 py-1.5 rounded-full">
                    <div className={cn(
                        "w-2 h-2 rounded-full",
                        isVBEnabled ? "bg-green-500 animate-pulse" : "bg-gray-400"
                    )} />
                    <span className="text-xs text-white">
                        {isVBEnabled ? 'Virtual BG Active' : 'Camera Active'}
                    </span>
                </div>
            )}
        </div>
    );
}
