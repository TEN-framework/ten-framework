'use client';

import { useState, useEffect } from 'react';
import { Phone, PhoneOff, Clock, CheckCircle, XCircle } from 'lucide-react';
import { twilioAPI, CallInfo } from '../app/api';

interface CallStatusProps {
    callSid: string | null;
    onCallEnd: () => void;
}

export default function CallStatus({ callSid, onCallEnd }: CallStatusProps) {
    const [callInfo, setCallInfo] = useState<CallInfo | null>(null);
    const [isLoading, setIsLoading] = useState(false);

    useEffect(() => {
        if (!callSid) return;

        const fetchCallInfo = async () => {
            try {
                setIsLoading(true);
                const info = await twilioAPI.getCall(callSid);
                setCallInfo(info);
            } catch (error) {
                console.error('Error fetching call info:', error);
            } finally {
                setIsLoading(false);
            }
        };

        fetchCallInfo();

        // 每5秒更新一次状态
        const interval = setInterval(fetchCallInfo, 5000);

        return () => clearInterval(interval);
    }, [callSid]);

    const handleEndCall = async () => {
        if (!callSid) return;

        try {
            setIsLoading(true);
            await twilioAPI.deleteCall(callSid);
            onCallEnd();
        } catch (error) {
            console.error('Error ending call:', error);
        } finally {
            setIsLoading(false);
        }
    };

    const getStatusIcon = (status: string) => {
        switch (status) {
            case 'queued':
            case 'ringing':
                return <Clock className="w-5 h-5 text-yellow-500" />;
            case 'in-progress':
                return <Phone className="w-5 h-5 text-green-500" />;
            case 'completed':
                return <CheckCircle className="w-5 h-5 text-green-500" />;
            case 'failed':
            case 'busy':
            case 'no-answer':
                return <XCircle className="w-5 h-5 text-red-500" />;
            default:
                return <Phone className="w-5 h-5 text-gray-500" />;
        }
    };

    const getStatusText = (status: string) => {
        switch (status) {
            case 'queued':
                return 'Queued';
            case 'ringing':
                return 'Ringing';
            case 'in-progress':
                return 'In Progress';
            case 'completed':
                return 'Completed';
            case 'failed':
                return 'Failed';
            case 'busy':
                return 'Busy';
            case 'no-answer':
                return 'No Answer';
            default:
                return status;
        }
    };

    if (!callSid || !callInfo) {
        return null;
    }

    return (
        <div className="bg-white rounded-lg shadow-md p-6">
            <h3 className="text-lg font-semibold text-gray-900 mb-4 flex items-center">
                <Phone className="w-5 h-5 mr-2 text-blue-600" />
                Call Status
            </h3>

            <div className="space-y-4">
                <div className="flex items-center justify-between">
                    <span className="text-sm font-medium text-gray-700">Call ID:</span>
                    <span className="text-sm text-gray-900 font-mono">{callInfo.call_sid}</span>
                </div>

                <div className="flex items-center justify-between">
                    <span className="text-sm font-medium text-gray-700">Phone Number:</span>
                    <span className="text-sm text-gray-900">{callInfo.phone_number}</span>
                </div>

                <div className="flex items-center justify-between">
                    <span className="text-sm font-medium text-gray-700">Status:</span>
                    <div className="flex items-center">
                        {getStatusIcon(callInfo.status)}
                        <span className="text-sm text-gray-900 ml-2">{getStatusText(callInfo.status)}</span>
                    </div>
                </div>

                <div className="flex items-center justify-between">
                    <span className="text-sm font-medium text-gray-700">WebSocket:</span>
                    <span className={`text-sm ${callInfo.has_websocket ? 'text-green-600' : 'text-red-600'}`}>
                        {callInfo.has_websocket ? 'Connected' : 'Disconnected'}
                    </span>
                </div>

                <div className="flex items-center justify-between">
                    <span className="text-sm font-medium text-gray-700">Created At:</span>
                    <span className="text-sm text-gray-900">
                        {new Date(callInfo.created_at * 1000).toLocaleString()}
                    </span>
                </div>

                {callInfo.status === 'in-progress' && (
                    <div className="pt-4 border-t">
                        <button
                            onClick={handleEndCall}
                            disabled={isLoading}
                            className="btn-danger w-full disabled:opacity-50 disabled:cursor-not-allowed flex items-center justify-center"
                        >
                            {isLoading ? (
                                <>
                                    <div className="animate-spin rounded-full h-4 w-4 border-b-2 border-white mr-2"></div>
                                    Ending...
                                </>
                            ) : (
                                <>
                                    <PhoneOff className="w-4 h-4 mr-2" />
                                    End Call
                                </>
                            )}
                        </button>
                    </div>
                )}
            </div>
        </div>
    );
}
