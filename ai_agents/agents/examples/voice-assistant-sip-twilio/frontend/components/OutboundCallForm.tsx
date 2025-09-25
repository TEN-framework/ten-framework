'use client';

import { useState } from 'react';
import { Phone, MessageSquare, PhoneOff } from 'lucide-react';
import { CallResponse } from '../app/api';

interface OutboundCallFormProps {
    onCall: (phoneNumber: string, message: string) => void;
    onHangUp: () => void;
    isLoading: boolean;
    activeCall: CallResponse | null;
}

export default function OutboundCallForm({ onCall, onHangUp, isLoading, activeCall }: OutboundCallFormProps) {
    const [phoneNumber, setPhoneNumber] = useState('');
    const [message, setMessage] = useState('Hello, this is a call from the AI assistant.');

    const handleSubmit = (e: React.FormEvent) => {
        e.preventDefault();
        if (!phoneNumber.trim()) return;
        onCall(phoneNumber.trim(), message.trim());
    };

    const handleHangUp = () => {
        onHangUp();
    };

    return (
        <div className="bg-white rounded-lg shadow-md p-6">
            <h2 className="text-xl font-semibold text-gray-900 mb-4 flex items-center">
                <Phone className="w-5 h-5 mr-2 text-blue-600" />
                Initiate Outbound Call
            </h2>

            <form onSubmit={handleSubmit} className="space-y-4">
                <div>
                    <label htmlFor="outboundPhoneNumber" className="block text-sm font-medium text-gray-700 mb-2">
                        Phone Number
                    </label>
                    <input
                        type="tel"
                        id="outboundPhoneNumber"
                        value={phoneNumber}
                        onChange={(e) => setPhoneNumber(e.target.value)}
                        placeholder="Enter phone number (e.g., +1234567890)"
                        className="input-field"
                        required
                    />
                    <p className="text-xs text-gray-500 mt-1">
                        Please enter the complete phone number including country code
                    </p>
                </div>

                <div>
                    <label htmlFor="outboundMessage" className="block text-sm font-medium text-gray-700 mb-2 flex items-center">
                        <MessageSquare className="w-4 h-4 mr-1" />
                        Call Message
                    </label>
                    <textarea
                        id="outboundMessage"
                        value={message}
                        onChange={(e) => setMessage(e.target.value)}
                        placeholder="Enter the message to be played"
                        rows={3}
                        className="input-field resize-none"
                    />
                    <p className="text-xs text-gray-500 mt-1">
                        This is the message that the AI assistant will play at the beginning of the call
                    </p>
                </div>

                {activeCall ? (
                    <button
                        type="button"
                        onClick={handleHangUp}
                        disabled={isLoading}
                        className="btn-danger w-full disabled:opacity-50 disabled:cursor-not-allowed flex items-center justify-center"
                    >
                        {isLoading ? (
                            <>
                                <div className="animate-spin rounded-full h-4 w-4 border-b-2 border-white mr-2"></div>
                                Hanging up...
                            </>
                        ) : (
                            <>
                                <PhoneOff className="w-4 h-4 mr-2" />
                                Hang Up Call
                            </>
                        )}
                    </button>
                ) : (
                    <button
                        type="submit"
                        disabled={!phoneNumber.trim() || isLoading}
                        className="btn-primary w-full disabled:opacity-50 disabled:cursor-not-allowed flex items-center justify-center"
                    >
                        {isLoading ? (
                            <>
                                <div className="animate-spin rounded-full h-4 w-4 border-b-2 border-white mr-2"></div>
                                Dialing...
                            </>
                        ) : (
                            <>
                                <Phone className="w-4 h-4 mr-2" />
                                Initiate Outbound Call
                            </>
                        )}
                    </button>
                )}
            </form>
        </div>
    );
}
