'use client';

import { X, Phone } from 'lucide-react';

interface InboundCallModalProps {
    isOpen: boolean;
    onClose: () => void;
    fromNumber: string;
}

export default function InboundCallModal({ isOpen, onClose, fromNumber }: InboundCallModalProps) {
    if (!isOpen) return null;

    return (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
            <div className="bg-white rounded-lg shadow-xl max-w-md w-full mx-4">
                <div className="flex items-center justify-between p-6 border-b">
                    <h2 className="text-xl font-semibold text-gray-900 flex items-center">
                        <Phone className="w-5 h-5 mr-2 text-green-600" />
                        Incoming Call
                    </h2>
                    <button
                        onClick={onClose}
                        className="text-gray-400 hover:text-gray-600 transition-colors"
                    >
                        <X className="w-6 h-6" />
                    </button>
                </div>

                <div className="p-6">
                    <div className="text-center">
                        <div className="mb-4">
                            <Phone className="w-16 h-16 text-green-600 mx-auto mb-4" />
                            <p className="text-lg text-gray-600 mb-2">Call from:</p>
                            <p className="text-2xl font-semibold text-gray-900">{fromNumber}</p>
                        </div>

                        <p className="text-sm text-gray-500">
                            This is an incoming call notification. The call is being handled automatically.
                        </p>
                    </div>
                </div>
            </div>
        </div>
    );
}
