"use client";

import { MessageSquare, Phone, PhoneOff } from "lucide-react";
import { useState } from "react";
import type { CallResponse } from "../app/api";

interface OutboundCallFormProps {
  onCall: (phoneNumber: string, message: string) => void;
  onHangUp: () => void;
  isLoading: boolean;
  activeCall: CallResponse | null;
}

export default function OutboundCallForm({
  onCall,
  onHangUp,
  isLoading,
  activeCall,
}: OutboundCallFormProps) {
  const [phoneNumber, setPhoneNumber] = useState("");
  const [message, setMessage] = useState(
    "Hello, this is a call from the AI assistant."
  );

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    if (!phoneNumber.trim()) return;
    onCall(phoneNumber.trim(), message.trim());
  };

  const handleHangUp = () => {
    onHangUp();
  };

  return (
    <div className="rounded-lg bg-white p-6 shadow-md">
      <h2 className="mb-4 flex items-center font-semibold text-gray-900 text-xl">
        <Phone className="mr-2 h-5 w-5 text-blue-600" />
        Initiate Outbound Call
      </h2>

      <form onSubmit={handleSubmit} className="space-y-4">
        <div>
          <label
            htmlFor="outboundPhoneNumber"
            className="mb-2 block font-medium text-gray-700 text-sm"
          >
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
          <p className="mt-1 text-gray-500 text-xs">
            Please enter the complete phone number including country code
          </p>
        </div>

        <div>
          <label
            htmlFor="outboundMessage"
            className="mb-2 block flex items-center font-medium text-gray-700 text-sm"
          >
            <MessageSquare className="mr-1 h-4 w-4" />
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
          <p className="mt-1 text-gray-500 text-xs">
            This is the message that the AI assistant will play at the beginning
            of the call
          </p>
        </div>

        {activeCall ? (
          <button
            type="button"
            onClick={handleHangUp}
            disabled={isLoading}
            className="btn-danger flex w-full items-center justify-center disabled:cursor-not-allowed disabled:opacity-50"
          >
            {isLoading ? (
              <>
                <div className="mr-2 h-4 w-4 animate-spin rounded-full border-white border-b-2"></div>
                Hanging up...
              </>
            ) : (
              <>
                <PhoneOff className="mr-2 h-4 w-4" />
                Hang Up Call
              </>
            )}
          </button>
        ) : (
          <button
            type="submit"
            disabled={!phoneNumber.trim() || isLoading}
            className="btn-primary flex w-full items-center justify-center disabled:cursor-not-allowed disabled:opacity-50"
          >
            {isLoading ? (
              <>
                <div className="mr-2 h-4 w-4 animate-spin rounded-full border-white border-b-2"></div>
                Dialing...
              </>
            ) : (
              <>
                <Phone className="mr-2 h-4 w-4" />
                Initiate Outbound Call
              </>
            )}
          </button>
        )}
      </form>
    </div>
  );
}
