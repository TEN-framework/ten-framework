"use client";

import { Phone, X } from "lucide-react";

interface InboundCallModalProps {
  isOpen: boolean;
  onClose: () => void;
  fromNumber: string;
}

export default function InboundCallModal({
  isOpen,
  onClose,
  fromNumber,
}: InboundCallModalProps) {
  if (!isOpen) return null;

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black bg-opacity-50">
      <div className="mx-4 w-full max-w-md rounded-lg bg-white shadow-xl">
        <div className="flex items-center justify-between border-b p-6">
          <h2 className="flex items-center font-semibold text-gray-900 text-xl">
            <Phone className="mr-2 h-5 w-5 text-green-600" />
            Incoming Call
          </h2>
          <button
            onClick={onClose}
            className="text-gray-400 transition-colors hover:text-gray-600"
          >
            <X className="h-6 w-6" />
          </button>
        </div>

        <div className="p-6">
          <div className="text-center">
            <div className="mb-4">
              <Phone className="mx-auto mb-4 h-16 w-16 text-green-600" />
              <p className="mb-2 text-gray-600 text-lg">Call from:</p>
              <p className="font-semibold text-2xl text-gray-900">
                {fromNumber}
              </p>
            </div>

            <p className="text-gray-500 text-sm">
              This is an incoming call notification. The call is being handled
              automatically.
            </p>
          </div>
        </div>
      </div>
    </div>
  );
}
