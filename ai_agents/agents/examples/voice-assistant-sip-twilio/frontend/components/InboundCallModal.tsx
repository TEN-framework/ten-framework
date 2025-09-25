'use client';

import { useState } from 'react';
import { X, Phone } from 'lucide-react';

interface InboundCallModalProps {
    isOpen: boolean;
    onClose: () => void;
    onCall: (phoneNumber: string) => void;
}

export default function InboundCallModal({ isOpen, onClose, onCall }: InboundCallModalProps) {
    const [phoneNumber, setPhoneNumber] = useState('');
    const [isLoading, setIsLoading] = useState(false);

    const handleSubmit = async (e: React.FormEvent) => {
        e.preventDefault();
        if (!phoneNumber.trim()) return;

        setIsLoading(true);
        try {
            await onCall(phoneNumber.trim());
            setPhoneNumber('');
            onClose();
        } catch (error) {
            console.error('Error making inbound call:', error);
        } finally {
            setIsLoading(false);
        }
    };

    const handleClose = () => {
        setPhoneNumber('');
        onClose();
    };

    if (!isOpen) return null;

    return (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
            <div className="bg-white rounded-lg shadow-xl max-w-md w-full mx-4">
                <div className="flex items-center justify-between p-6 border-b">
                    <h2 className="text-xl font-semibold text-gray-900 flex items-center">
                        <Phone className="w-5 h-5 mr-2 text-blue-600" />
                        Initiate Inbound Call
                    </h2>
                    <button
                        onClick={handleClose}
                        className="text-gray-400 hover:text-gray-600 transition-colors"
                    >
                        <X className="w-6 h-6" />
                    </button>
                </div>

                <form onSubmit={handleSubmit} className="p-6">
                    <div className="mb-4">
                        <label htmlFor="phoneNumber" className="block text-sm font-medium text-gray-700 mb-2">
                            Phone Number
                        </label>
                        <input
                            type="tel"
                            id="phoneNumber"
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

                    <div className="flex justify-end space-x-3">
                        <button
                            type="button"
                            onClick={handleClose}
                            className="px-4 py-2 text-gray-700 bg-gray-100 hover:bg-gray-200 rounded-lg transition-colors"
                        >
                            Cancel
                        </button>
                        <button
                            type="submit"
                            disabled={!phoneNumber.trim() || isLoading}
                            className="btn-primary disabled:opacity-50 disabled:cursor-not-allowed flex items-center"
                        >
                            {isLoading ? (
                                <>
                                    <div className="animate-spin rounded-full h-4 w-4 border-b-2 border-white mr-2"></div>
                                    Dialing...
                                </>
                            ) : (
                                <>
                                    <Phone className="w-4 h-4 mr-2" />
                                    Initiate Call
                                </>
                            )}
                        </button>
                    </div>
                </form>
            </div>
        </div>
    );
}
