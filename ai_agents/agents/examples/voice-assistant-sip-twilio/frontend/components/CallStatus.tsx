"use client";

import { CheckCircle, Clock, Phone, PhoneOff, XCircle } from "lucide-react";
import { useEffect, useState } from "react";
import { type CallInfo, twilioAPI } from "../app/api";

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
        console.error("Error fetching call info:", error);
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
      console.error("Error ending call:", error);
    } finally {
      setIsLoading(false);
    }
  };

  const getStatusIcon = (status: string) => {
    switch (status) {
      case "queued":
      case "ringing":
        return <Clock className="h-5 w-5 text-yellow-500" />;
      case "in-progress":
        return <Phone className="h-5 w-5 text-green-500" />;
      case "completed":
        return <CheckCircle className="h-5 w-5 text-green-500" />;
      case "failed":
      case "busy":
      case "no-answer":
        return <XCircle className="h-5 w-5 text-red-500" />;
      default:
        return <Phone className="h-5 w-5 text-gray-500" />;
    }
  };

  const getStatusText = (status: string) => {
    switch (status) {
      case "queued":
        return "Queued";
      case "ringing":
        return "Ringing";
      case "in-progress":
        return "In Progress";
      case "completed":
        return "Completed";
      case "failed":
        return "Failed";
      case "busy":
        return "Busy";
      case "no-answer":
        return "No Answer";
      default:
        return status;
    }
  };

  if (!callSid || !callInfo) {
    return null;
  }

  return (
    <div className="rounded-lg bg-white p-6 shadow-md">
      <h3 className="mb-4 flex items-center font-semibold text-gray-900 text-lg">
        <Phone className="mr-2 h-5 w-5 text-blue-600" />
        Call Status
      </h3>

      <div className="space-y-4">
        <div className="flex items-center justify-between">
          <span className="font-medium text-gray-700 text-sm">Call ID:</span>
          <span className="font-mono text-gray-900 text-sm">
            {callInfo.call_sid}
          </span>
        </div>

        <div className="flex items-center justify-between">
          <span className="font-medium text-gray-700 text-sm">
            Phone Number:
          </span>
          <span className="text-gray-900 text-sm">{callInfo.phone_number}</span>
        </div>

        <div className="flex items-center justify-between">
          <span className="font-medium text-gray-700 text-sm">Status:</span>
          <div className="flex items-center">
            {getStatusIcon(callInfo.status)}
            <span className="ml-2 text-gray-900 text-sm">
              {getStatusText(callInfo.status)}
            </span>
          </div>
        </div>

        <div className="flex items-center justify-between">
          <span className="font-medium text-gray-700 text-sm">WebSocket:</span>
          <span
            className={`text-sm ${callInfo.has_websocket ? "text-green-600" : "text-red-600"}`}
          >
            {callInfo.has_websocket ? "Connected" : "Disconnected"}
          </span>
        </div>

        <div className="flex items-center justify-between">
          <span className="font-medium text-gray-700 text-sm">Created At:</span>
          <span className="text-gray-900 text-sm">
            {new Date(callInfo.created_at * 1000).toLocaleString()}
          </span>
        </div>

        {callInfo.status === "in-progress" && (
          <div className="border-t pt-4">
            <button
              onClick={handleEndCall}
              disabled={isLoading}
              className="btn-danger flex w-full items-center justify-center disabled:cursor-not-allowed disabled:opacity-50"
            >
              {isLoading ? (
                <>
                  <div className="mr-2 h-4 w-4 animate-spin rounded-full border-white border-b-2"></div>
                  Ending...
                </>
              ) : (
                <>
                  <PhoneOff className="mr-2 h-4 w-4" />
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
