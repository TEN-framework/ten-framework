'use client';

import { useState } from 'react';
import { Phone, PhoneIncoming, PhoneOutgoing } from 'lucide-react';
import OutboundCallForm from '@/components/OutboundCallForm';
import InboundCallModal from '@/components/InboundCallModal';
import CallStatus from '@/components/CallStatus';
import { twilioAPI, CallResponse } from '@/lib/api';

export default function Home() {
    const [activeCall, setActiveCall] = useState<CallResponse | null>(null);
    const [isOutboundLoading, setIsOutboundLoading] = useState(false);
    const [isInboundModalOpen, setIsInboundModalOpen] = useState(false);
    const [error, setError] = useState<string | null>(null);

    const handleOutboundCall = async (phoneNumber: string, message: string) => {
        try {
            setIsOutboundLoading(true);
            setError(null);

            const response = await twilioAPI.createCall({
                phone_number: phoneNumber,
                message: message,
            });

            setActiveCall(response);
        } catch (error: any) {
            console.error('Error creating outbound call:', error);
            setError(error.response?.data?.detail || 'Failed to create outbound call');
        } finally {
            setIsOutboundLoading(false);
        }
    };

    const handleInboundCall = async (phoneNumber: string) => {
        try {
            setError(null);

            const response = await twilioAPI.createCall({
                phone_number: phoneNumber,
                message: 'Hello, this is an inbound call from the AI assistant.',
            });

            setActiveCall(response);
        } catch (error: any) {
            console.error('Error creating inbound call:', error);
            setError(error.response?.data?.detail || 'Failed to create inbound call');
        }
    };

    const handleCallEnd = () => {
        setActiveCall(null);
    };

    return (
        <div className="space-y-8">
            {/* Header */}
            <div className="text-center">
                <h1 className="text-4xl font-bold text-gray-900 mb-4">
                    Twilio Voice Assistant
                </h1>
                <p className="text-lg text-gray-600">
                    AI-powered voice assistant for outbound and inbound calls
                </p>
            </div>

            {/* Error Display */}
            {error && (
                <div className="bg-red-50 border border-red-200 rounded-lg p-4">
                    <div className="flex">
                        <div className="flex-shrink-0">
                            <svg className="h-5 w-5 text-red-400" viewBox="0 0 20 20" fill="currentColor">
                                <path fillRule="evenodd" d="M10 18a8 8 0 100-16 8 8 0 000 16zM8.707 7.293a1 1 0 00-1.414 1.414L8.586 10l-1.293 1.293a1 1 0 101.414 1.414L10 11.414l1.293 1.293a1 1 0 001.414-1.414L11.414 10l1.293-1.293a1 1 0 00-1.414-1.414L10 8.586 8.707 7.293z" clipRule="evenodd" />
                            </svg>
                        </div>
                        <div className="ml-3">
                            <h3 className="text-sm font-medium text-red-800">Error</h3>
                            <div className="mt-2 text-sm text-red-700">
                                <p>{error}</p>
                            </div>
                        </div>
                    </div>
                </div>
            )}

            {/* Main Content */}
            <div className="grid grid-cols-1 lg:grid-cols-2 gap-8">
                {/* Outbound Call Section */}
                <div className="space-y-6">
                    <div className="flex items-center">
                        <PhoneOutgoing className="w-6 h-6 text-blue-600 mr-3" />
                        <h2 className="text-2xl font-semibold text-gray-900">Outbound Calls</h2>
                    </div>
                    <OutboundCallForm
                        onCall={handleOutboundCall}
                        isLoading={isOutboundLoading}
                    />
                </div>

                {/* Inbound Call Section */}
                <div className="space-y-6">
                    <div className="flex items-center">
                        <PhoneIncoming className="w-6 h-6 text-green-600 mr-3" />
                        <h2 className="text-2xl font-semibold text-gray-900">Inbound Calls</h2>
                    </div>

                    <div className="bg-white rounded-lg shadow-md p-6">
                        <p className="text-gray-600 mb-4">
                            Click the button below to initiate an inbound call. A dialog will pop up for you to enter the phone number.
                        </p>
                        <button
                            onClick={() => setIsInboundModalOpen(true)}
                            className="btn-success w-full flex items-center justify-center"
                        >
                            <PhoneIncoming className="w-5 h-5 mr-2" />
                            Initiate Inbound Call
                        </button>
                    </div>
                </div>
            </div>

            {/* Active Call Status */}
            {activeCall && (
                <div className="mt-8">
                    <CallStatus
                        callSid={activeCall.call_sid}
                        onCallEnd={handleCallEnd}
                    />
                </div>
            )}

            {/* Inbound Call Modal */}
            <InboundCallModal
                isOpen={isInboundModalOpen}
                onClose={() => setIsInboundModalOpen(false)}
                onCall={handleInboundCall}
            />
        </div>
    );
}
