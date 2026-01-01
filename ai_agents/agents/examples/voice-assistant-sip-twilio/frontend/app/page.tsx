"use client";

import { PhoneIncoming, PhoneOutgoing } from "lucide-react";
import { useEffect, useState } from "react";
import CallStatus from "@/components/CallStatus";
import InboundCallModal from "@/components/InboundCallModal";
import OutboundCallForm from "@/components/OutboundCallForm";
import { type CallResponse, type ServerConfig, twilioAPI } from "./api";

export default function Home() {
  const [activeCall, setActiveCall] = useState<CallResponse | null>(null);
  const [isOutboundLoading, setIsOutboundLoading] = useState(false);
  const [isInboundModalOpen, setIsInboundModalOpen] = useState(false);
  const [inboundFromNumber, setInboundFromNumber] = useState<string>("");
  const [error, setError] = useState<string | null>(null);
  const [serverConfig, setServerConfig] = useState<ServerConfig | null>(null);
  const [isConfigLoading, setIsConfigLoading] = useState(true);

  // Get Twilio from number from server config
  const twilioFromNumber = serverConfig?.twilio_from_number || "+1234567890";

  // Load server configuration on component mount
  useEffect(() => {
    const loadConfig = async () => {
      try {
        setIsConfigLoading(true);
        const config = await twilioAPI.getConfig();
        setServerConfig(config);
      } catch (error) {
        console.error("Failed to load server config:", error);
        // Use default values if config loading fails
        setServerConfig({
          twilio_from_number: "+1234567890",
          server_port: 8000,
          tenapp_port: 8080,
          tenapp_url: "http://localhost:8080",
          public_server_url: "http://localhost:8000",
          use_https: false,
          use_wss: false,
          media_stream_enabled: false,
          media_ws_url: "ws://localhost:8000/ws",
          webhook_enabled: false,
          webhook_url: "",
        });
      } finally {
        setIsConfigLoading(false);
      }
    };

    loadConfig();
  }, []);

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
      console.error("Error creating outbound call:", error);
      setError(
        error.response?.data?.detail || "Failed to create outbound call"
      );
    } finally {
      setIsOutboundLoading(false);
    }
  };

  const handleInboundCallNotification = (fromNumber: string) => {
    setInboundFromNumber(fromNumber);
    setIsInboundModalOpen(true);
  };

  const handleCallEnd = () => {
    setActiveCall(null);
  };

  const handleHangUp = async () => {
    if (!activeCall) return;

    try {
      setIsOutboundLoading(true);
      setError(null);

      await twilioAPI.deleteCall(activeCall.call_sid);
      setActiveCall(null);
    } catch (error: any) {
      console.error("Error hanging up call:", error);
      setError(error.response?.data?.detail || "Failed to hang up call");
    } finally {
      setIsOutboundLoading(false);
    }
  };

  return (
    <div className="space-y-8">
      {/* Header */}
      <div className="text-center">
        <h1 className="mb-4 font-bold text-4xl text-gray-900">
          Twilio Voice Assistant
        </h1>
        <p className="text-gray-600 text-lg">
          AI-powered voice assistant for outbound and inbound calls
        </p>
      </div>

      {/* Error Display */}
      {error && (
        <div className="rounded-lg border border-red-200 bg-red-50 p-4">
          <div className="flex">
            <div className="flex-shrink-0">
              <svg
                className="h-5 w-5 text-red-400"
                viewBox="0 0 20 20"
                fill="currentColor"
              >
                <path
                  fillRule="evenodd"
                  d="M10 18a8 8 0 100-16 8 8 0 000 16zM8.707 7.293a1 1 0 00-1.414 1.414L8.586 10l-1.293 1.293a1 1 0 101.414 1.414L10 11.414l1.293 1.293a1 1 0 001.414-1.414L11.414 10l1.293-1.293a1 1 0 00-1.414-1.414L10 8.586 8.707 7.293z"
                  clipRule="evenodd"
                />
              </svg>
            </div>
            <div className="ml-3">
              <h3 className="font-medium text-red-800 text-sm">Error</h3>
              <div className="mt-2 text-red-700 text-sm">
                <p>{error}</p>
              </div>
            </div>
          </div>
        </div>
      )}

      {/* Main Content */}
      <div className="grid grid-cols-1 gap-8 lg:grid-cols-2">
        {/* Outbound Call Section */}
        <div className="space-y-6">
          <div className="flex items-center">
            <PhoneOutgoing className="mr-3 h-6 w-6 text-blue-600" />
            <h2 className="font-semibold text-2xl text-gray-900">
              Outbound Calls
            </h2>
          </div>
          <OutboundCallForm
            onCall={handleOutboundCall}
            onHangUp={handleHangUp}
            isLoading={isOutboundLoading}
            activeCall={activeCall}
          />
        </div>

        {/* Inbound Call Section */}
        <div className="space-y-6">
          <div className="flex items-center">
            <PhoneIncoming className="mr-3 h-6 w-6 text-green-600" />
            <h2 className="font-semibold text-2xl text-gray-900">
              Inbound Calls
            </h2>
          </div>

          <div className="rounded-lg bg-white p-6 shadow-md">
            <p className="mb-4 text-gray-600">
              Click the button below to simulate an incoming call notification.
            </p>
            <button
              onClick={() => handleInboundCallNotification(twilioFromNumber)}
              disabled={isConfigLoading}
              className="btn-success flex w-full items-center justify-center disabled:cursor-not-allowed disabled:opacity-50"
            >
              {isConfigLoading ? (
                <>
                  <div className="mr-2 h-4 w-4 animate-spin rounded-full border-white border-b-2"></div>
                  Loading...
                </>
              ) : (
                <>
                  <PhoneIncoming className="mr-2 h-5 w-5" />
                  Simulate Inbound Call
                </>
              )}
            </button>
          </div>
        </div>
      </div>

      {/* Active Call Status */}
      {activeCall && (
        <div className="mt-8">
          <CallStatus callSid={activeCall.call_sid} onCallEnd={handleCallEnd} />
        </div>
      )}

      {/* Inbound Call Modal */}
      <InboundCallModal
        isOpen={isInboundModalOpen}
        onClose={() => setIsInboundModalOpen(false)}
        fromNumber={inboundFromNumber}
      />
    </div>
  );
}
