'use client';

import { useState, useEffect } from 'react';
import { createCall, endCall, listCalls, getConfig } from './api';

interface Config {
  telnyx_from_number: string;
  server_port: number;
  public_server_url: string | null;
  use_https: boolean;
  use_wss: boolean;
  media_stream_enabled: boolean;
  media_ws_url: string | null;
  webhook_enabled: boolean;
  webhook_url: string | null;
}

interface Call {
  call_id: string;
  status: string;
  phone_number: string;
  message: string;
  created_at: string;
  ended_at?: string;
}

export default function Home() {
  const [phoneNumber, setPhoneNumber] = useState('');
  const [message, setMessage] = useState('Hello from Telnyx voice assistant!');
  const [calls, setCalls] = useState<string[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [config, setConfig] = useState<Config | null>(null);

  useEffect(() => {
    loadConfig();
    loadCalls();
    const interval = setInterval(loadCalls, 5000);
    return () => clearInterval(interval);
  }, []);

  const loadConfig = async () => {
    try {
      const data = await getConfig();
      setConfig(data);
    } catch (err) {
      console.error('Failed to load config:', err);
    }
  };

  const loadCalls = async () => {
    try {
      const data = await listCalls();
      setCalls(data.calls);
    } catch (err) {
      console.error('Failed to load calls:', err);
    }
  };

  const handleCreateCall = async (e: React.FormEvent) => {
    e.preventDefault();
    setLoading(true);
    setError(null);

    try {
      await createCall({ phone_number: phoneNumber, message });
      setPhoneNumber('');
      loadCalls();
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to create call');
    } finally {
      setLoading(false);
    }
  };

  const handleEndCall = async (callId: string) => {
    setLoading(true);
    setError(null);

    try {
      await endCall(callId);
      loadCalls();
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to end call');
    } finally {
      setLoading(false);
    }
  };

  return (
    <main className="min-h-screen p-8 bg-gray-100">
      <div className="max-w-4xl mx-auto">
        <h1 className="text-3xl font-bold mb-8">Telnyx Voice Assistant</h1>

        {config && (
          <div className="bg-white rounded-lg shadow p-6 mb-8">
            <h2 className="text-xl font-semibold mb-4">Configuration</h2>
            <div className="grid grid-cols-2 gap-4 text-sm">
              <div>
                <span className="font-medium">From Number:</span> {config.telnyx_from_number}
              </div>
              <div>
                <span className="font-medium">Server Port:</span> {config.server_port}
              </div>
              <div>
                <span className="font-medium">Media Stream:</span> {config.media_stream_enabled ? 'Enabled' : 'Disabled'}
              </div>
              <div>
                <span className="font-medium">Webhooks:</span> {config.webhook_enabled ? 'Enabled' : 'Disabled'}
              </div>
            </div>
          </div>
        )}

        <div className="bg-white rounded-lg shadow p-6 mb-8">
          <h2 className="text-xl font-semibold mb-4">Start New Call</h2>
          <form onSubmit={handleCreateCall} className="space-y-4">
            <div>
              <label className="block text-sm font-medium text-gray-700 mb-1">
                Phone Number
              </label>
              <input
                type="tel"
                value={phoneNumber}
                onChange={(e) => setPhoneNumber(e.target.value)}
                placeholder="+1234567890"
                className="w-full px-4 py-2 border rounded-lg focus:ring-2 focus:ring-blue-500"
                required
              />
            </div>
            <div>
              <label className="block text-sm font-medium text-gray-700 mb-1">
                Message (optional)
              </label>
              <input
                type="text"
                value={message}
                onChange={(e) => setMessage(e.target.value)}
                placeholder="Hello from Telnyx voice assistant!"
                className="w-full px-4 py-2 border rounded-lg focus:ring-2 focus:ring-blue-500"
              />
            </div>
            <button
              type="submit"
              disabled={loading}
              className="w-full bg-blue-600 text-white py-2 px-4 rounded-lg hover:bg-blue-700 disabled:bg-gray-400"
            >
              {loading ? 'Starting...' : 'Start Call'}
            </button>
            {error && <p className="text-red-500 text-sm">{error}</p>}
          </form>
        </div>

        <div className="bg-white rounded-lg shadow p-6">
          <h2 className="text-xl font-semibold mb-4">Active Calls ({calls.length})</h2>
          {calls.length === 0 ? (
            <p className="text-gray-500">No active calls</p>
          ) : (
            <ul className="divide-y">
              {calls.map((callId) => (
                <li key={callId} className="py-4 flex justify-between items-center">
                  <span className="font-mono">{callId}</span>
                  <button
                    onClick={() => handleEndCall(callId)}
                    disabled={loading}
                    className="bg-red-500 text-white py-1 px-3 rounded hover:bg-red-600 disabled:bg-gray-400 text-sm"
                  >
                    End Call
                  </button>
                </li>
              ))}
            </ul>
          )}
        </div>
      </div>
    </main>
  );
}