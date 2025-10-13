'use client';

import React, { useState, useEffect } from 'react';

// Force dynamic rendering
export const dynamic = 'force-dynamic';
import dynamicImport from 'next/dynamic';

// Dynamically import Live2D component to prevent SSR issues
const ClientOnlyLive2D = dynamicImport(() => import('@/components/ClientOnlyLive2D'), {
    ssr: false,
    loading: () => (
        <div className="flex items-center justify-center h-full">
            <div className="text-center">
                <div className="animate-spin rounded-full h-12 w-12 border-b-2 border-white mx-auto mb-4"></div>
                <p className="text-white/70">Loading Live2D Model...</p>
            </div>
        </div>
    )
});

import { Live2DModel, AgoraConfig } from '@/types';
import { apiStartService, apiStopService, apiPing } from '@/lib/request';

// Use Kei model with MotionSync support
const defaultModel: Live2DModel = {
    id: 'kei',
    name: 'Kei',
    path: '/models/kei_vowels_pro/kei_vowels_pro.model3.json',
    preview: '/models/kei_vowels_pro/preview.png'
};

export default function Home() {
    const [isConnected, setIsConnected] = useState(false);
    const [isMuted, setIsMuted] = useState(false);
    const [isConnecting, setIsConnecting] = useState(false);
    const [selectedModel, setSelectedModel] = useState<Live2DModel>(defaultModel);
    const [remoteAudioTrack, setRemoteAudioTrack] = useState<any>(null);
    const [agoraService, setAgoraService] = useState<any>(null);
    const [pingInterval, setPingInterval] = useState<NodeJS.Timeout | null>(null);

    useEffect(() => {
        // Dynamically import Agora service only on client side
        if (typeof window !== 'undefined') {
            import('@/services/agora').then((module) => {
                const service = module.agoraService;
                setAgoraService(service);

                // Set up callbacks for Agora service
                service.setOnConnectionStatusChange(handleConnectionChange);
                service.setOnRemoteAudioTrack(handleAudioTrackChange);
            });
        }

        // Cleanup ping interval on unmount
        return () => {
            stopPing();
        };
    }, []);

    const handleConnectionChange = (status: any) => {
        setIsConnected(status.rtc === 'connected');
    };

    const handleAudioTrackChange = (track: any) => {
        setRemoteAudioTrack(track);
    };

    const startPing = () => {
        if (pingInterval) {
            stopPing();
        }
        const interval = setInterval(() => {
            apiPing('test-channel');
        }, 3000);
        setPingInterval(interval);
    };

    const stopPing = () => {
        if (pingInterval) {
            clearInterval(pingInterval);
            setPingInterval(null);
        }
    };

    const handleMicToggle = () => {
        if (agoraService) {
            try {
                if (isMuted) {
                    agoraService.unmuteMicrophone();
                    setIsMuted(false);
                } else {
                    agoraService.muteMicrophone();
                    setIsMuted(true);
                }
            } catch (error) {
                console.error('Error toggling microphone:', error);
            }
        }
    };

    const handleConnectToggle = async () => {
        if (agoraService) {
            try {
                if (isConnected) {
                    setIsConnecting(true);
                    // Stop the agent service first
                    try {
                        await apiStopService('test-channel');
                        console.log('Agent stopped');
                    } catch (error) {
                        console.error('Failed to stop agent:', error);
                    }

                    await agoraService.disconnect();
                    setIsConnected(false);
                    stopPing(); // Stop ping when disconnecting
                    setIsConnecting(false);
                } else {
                    setIsConnecting(true);
                    // Fetch Agora credentials from API server using the correct endpoint
                    const response = await fetch('/api/token/generate', {
                        method: 'POST',
                        headers: {
                            'Content-Type': 'application/json',
                        },
                        body: JSON.stringify({
                            request_id: Math.random().toString(36).substring(2, 15),
                            uid: Math.floor(Math.random() * 100000),
                            channel_name: 'test-channel'
                        })
                    });

                    if (!response.ok) {
                        throw new Error(`Failed to get Agora credentials: ${response.statusText}`);
                    }

                    const responseData = await response.json();

                    // Handle the response structure from agent server
                    const credentials = responseData.data || responseData;

                    const agoraConfig: AgoraConfig = {
                        appId: credentials.appId || credentials.app_id,
                        channel: credentials.channel_name,
                        token: credentials.token,
                        uid: credentials.uid
                    };

                    console.log('Agora config:', agoraConfig);
                    const success = await agoraService.connect(agoraConfig);
                    if (success) {
                        setIsConnected(true);

                        // Sync microphone state with Agora service
                        setIsMuted(agoraService.isMicrophoneMuted());

                        // Start the agent service
                        try {
                            const startResult = await apiStartService({
                                channel: agoraConfig.channel,
                                userId: agoraConfig.uid || 0,
                                graphName: 'voice_assistant',
                                language: 'en',
                                voiceType: 'female'
                            });

                            console.log('Agent started:', startResult);
                            startPing(); // Start ping when agent is started
                        } catch (error) {
                            console.error('Failed to start agent:', error);
                        }
                    } else {
                        console.error('Failed to connect to Agora');
                    }
                    setIsConnecting(false);
                }
            } catch (error) {
                console.error('Error toggling connection:', error);
                setIsConnecting(false);
            }
        }
    };


    return (
        <div className="min-h-screen bg-gradient-to-b from-slate-900 via-blue-900 to-slate-800 relative overflow-hidden flex justify-center">
            <div className="w-full max-w-md relative">
                {/* Night Sky Background */}
                <div className="absolute inset-0 bg-gradient-to-b from-slate-900 via-blue-900 to-slate-800">
                    {/* Uniform Star Field */}
                    <div className="absolute inset-0" style={{
                        backgroundImage: `radial-gradient(1px 1px at 20px 30px, #fff, transparent),
                                    radial-gradient(1px 1px at 40px 70px, #fff, transparent),
                                    radial-gradient(1px 1px at 60px 20px, #fff, transparent),
                                    radial-gradient(1px 1px at 80px 50px, #fff, transparent),
                                    radial-gradient(1px 1px at 100px 10px, #fff, transparent),
                                    radial-gradient(1px 1px at 120px 60px, #fff, transparent),
                                    radial-gradient(1px 1px at 140px 30px, #fff, transparent),
                                    radial-gradient(1px 1px at 160px 80px, #fff, transparent),
                                    radial-gradient(1px 1px at 180px 40px, #fff, transparent),
                                    radial-gradient(1px 1px at 200px 70px, #fff, transparent)`,
                        backgroundRepeat: 'repeat',
                        backgroundSize: '220px 100px'
                    }}></div>

                    {/* Subtle Mountain Silhouettes */}
                    <div className="absolute bottom-0 left-0 right-0 h-24 bg-gradient-to-t from-slate-800/30 to-transparent">
                        <div className="absolute bottom-0 left-0 w-full h-16 bg-slate-800/20" style={{
                            clipPath: 'polygon(0% 100%, 0% 80%, 20% 60%, 40% 70%, 60% 50%, 80% 60%, 100% 40%, 100% 100%)'
                        }}></div>
                    </div>
                </div>

                {/* Floating Stars */}
                <div className="absolute inset-0 pointer-events-none">
                    <div className="absolute top-16 left-1/4 text-yellow-400 text-2xl animate-bounce" style={{ animationDelay: '0s' }}>✨</div>
                    <div className="absolute top-24 right-1/3 text-yellow-400 text-xl animate-bounce" style={{ animationDelay: '1s' }}>✨</div>
                    <div className="absolute top-32 left-1/3 text-yellow-400 text-lg animate-bounce" style={{ animationDelay: '2s' }}>✨</div>
                    <div className="absolute top-20 right-1/4 text-yellow-400 text-xl animate-bounce" style={{ animationDelay: '0.5s' }}>✨</div>
                    <div className="absolute top-36 left-1/2 text-yellow-400 text-lg animate-bounce" style={{ animationDelay: '1.5s' }}>✨</div>
                </div>


                {/* Centered Character Display */}
                <main className="absolute inset-0 z-10 flex items-center justify-center pt-8 pb-32">
                    <div className="w-64 h-80 relative">
                        {/* Subtle glow effect around avatar */}
                        <div className="absolute inset-0 bg-gradient-to-b from-blue-400/20 via-transparent to-purple-400/20 rounded-2xl blur-sm"></div>
                        <ClientOnlyLive2D
                            key={selectedModel.id}
                            modelPath={selectedModel.path}
                            audioTrack={remoteAudioTrack}
                            className="h-full w-full rounded-2xl overflow-hidden shadow-2xl border border-white/10"
                        />
                        {/* Soft vignette effect */}
                        <div className="absolute inset-0 bg-gradient-to-t from-transparent via-transparent to-blue-900/10 rounded-2xl pointer-events-none"></div>
                    </div>
                </main>


                {/* Bottom Control Buttons */}
                <div className="absolute bottom-8 left-0 right-0 z-20">
                    <div className="flex items-center justify-center gap-6">
                        {/* Glow effect behind buttons */}
                        <div className="absolute inset-0 flex items-center justify-center gap-6 pointer-events-none">
                            <div className="w-14 h-14 rounded-full bg-white/5 blur-xl"></div>
                            <div className="w-14 h-14 rounded-full bg-white/5 blur-xl"></div>
                        </div>
                        {/* Mic Button */}
                        <button
                            onClick={handleMicToggle}
                            disabled={!isConnected}
                            className={`relative w-14 h-14 rounded-full flex items-center justify-center backdrop-blur-md border border-white/20 transition-all duration-300 shadow-lg overflow-hidden ${!isConnected
                                ? 'bg-gray-500/20 cursor-not-allowed opacity-50'
                                : isMuted
                                    ? 'bg-red-500/30 hover:bg-red-500/40 shadow-red-500/25 hover:scale-105 active:scale-95'
                                    : 'bg-white/10 hover:bg-white/15 shadow-white/10 hover:scale-105 active:scale-95'
                                }`}
                        >
                            {/* Subtle inner glow */}
                            <div className={`absolute inset-1 rounded-full ${!isConnected ? 'bg-gray-400/5' : isMuted ? 'bg-red-400/10' : 'bg-white/5'}`}></div>
                            {/* Button content */}
                            <div className="relative z-10">
                                {isMuted ? (
                                    // Muted mic icon
                                    <svg className="w-6 h-6 text-white" fill="currentColor" viewBox="0 0 24 24">
                                        <path d="M12 14c1.66 0 3-1.34 3-3V5c0-1.66-1.34-3-3-3S9 3.34 9 5v6c0 1.66 1.34 3 3 3z" />
                                        <path d="M17 11c0 2.76-2.24 5-5 5s-5-2.24-5-5H5c0 3.53 2.61 6.43 6 6.92V21h2v-3.08c3.39-.49 6-3.39 6-6.92h-2z" />
                                        <path d="M3 3l18 18" stroke="currentColor" strokeWidth="2" strokeLinecap="round" />
                                    </svg>
                                ) : (
                                    // Normal mic icon
                                    <svg className={`w-6 h-6 ${!isConnected ? 'text-white/40' : 'text-white/80'}`} fill="currentColor" viewBox="0 0 24 24">
                                        <path d="M12 14c1.66 0 3-1.34 3-3V5c0-1.66-1.34-3-3-3S9 3.34 9 5v6c0 1.66 1.34 3 3 3z" />
                                        <path d="M17 11c0 2.76-2.24 5-5 5s-5-2.24-5-5H5c0 3.53 2.61 6.43 6 6.92V21h2v-3.08c3.39-.49 6-3.39 6-6.92h-2z" />
                                    </svg>
                                )}
                            </div>
                        </button>

                        {/* Connect/Disconnect Button */}
                        <button
                            onClick={handleConnectToggle}
                            disabled={isConnecting}
                            className={`relative w-14 h-14 rounded-full flex items-center justify-center backdrop-blur-md border border-white/20 transition-all duration-300 hover:scale-105 active:scale-95 shadow-xl overflow-hidden ${isConnecting
                                ? 'bg-blue-500/30 shadow-blue-500/30 cursor-not-allowed'
                                : isConnected
                                    ? 'bg-red-500/30 hover:bg-red-500/40 shadow-red-500/30'
                                    : 'bg-white/10 hover:bg-white/15 shadow-white/10'
                                }`}
                        >
                            {/* Subtle inner glow */}
                            <div className={`absolute inset-1 rounded-full ${isConnecting ? 'bg-blue-400/10' : isConnected ? 'bg-red-400/10' : 'bg-white/5'}`}></div>
                            {/* Button content */}
                            <div className="relative z-10">
                                {isConnecting ? (
                                    // Loading spinner
                                    <svg className="w-6 h-6 text-white animate-spin" fill="none" viewBox="0 0 24 24">
                                        <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4"></circle>
                                        <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
                                    </svg>
                                ) : isConnected ? (
                                    // Disconnect/Stop icon
                                    <svg className="w-8 h-8 text-white" fill="currentColor" viewBox="0 0 24 24">
                                        <rect x="6" y="6" width="12" height="12" rx="2" />
                                    </svg>
                                ) : (
                                    // Connect/Play icon
                                    <svg className="w-8 h-8 text-white/90" fill="currentColor" viewBox="0 0 24 24">
                                        <path d="M8 5v14l11-7z" />
                                    </svg>
                                )}
                            </div>
                        </button>
                    </div>
                </div>
            </div>
        </div>
    );
}
