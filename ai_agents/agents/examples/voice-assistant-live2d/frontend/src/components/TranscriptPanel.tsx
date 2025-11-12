'use client';

import React, { useState, useEffect, useRef } from 'react';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Switch } from '@/components/ui/switch';
import { Label } from '@/components/ui/label';
// agoraService will be imported dynamically
import { TranscriptMessage } from '@/types';
import {
    MessageSquare,
    Trash2,
    Download,
    User,
    Bot,
    Clock
} from 'lucide-react';

interface TranscriptPanelProps {
    className?: string;
}

export default function TranscriptPanel({ className }: TranscriptPanelProps) {
    const [messages, setMessages] = useState<TranscriptMessage[]>([]);
    const [isAutoScroll, setIsAutoScroll] = useState(true);
    const [isEnabled, setIsEnabled] = useState(true);
    const [agoraService, setAgoraService] = useState<any>(null);
    const messagesEndRef = useRef<HTMLDivElement>(null);

    useEffect(() => {
        // Dynamically import Agora service only on client side
        if (typeof window !== 'undefined') {
            import('@/services/agora').then((module) => {
                setAgoraService(module.agoraService);
            });
        }
    }, []);

    useEffect(() => {
        if (!agoraService) return;

        // Set up transcript message listener
        const unsubscribe = agoraService.addTranscriptListener((message: TranscriptMessage) => {
            if (!isEnabled) {
                return;
            }
            setMessages(prev => {
                const existingIndex = prev.findIndex((entry) => entry.id === message.id);
                const next = [...prev];
                if (existingIndex !== -1) {
                    next[existingIndex] = message;
                    return next;
                }
                next.push(message);
                return next;
            });
        });

        return () => {
            unsubscribe?.();
        };
    }, [agoraService, isEnabled]);

    useEffect(() => {
        if (isAutoScroll && messagesEndRef.current) {
            messagesEndRef.current.scrollIntoView({ behavior: 'smooth' });
        }
    }, [messages, isAutoScroll]);

    const clearMessages = () => {
        setMessages([]);
    };

    const exportTranscript = () => {
        const transcript = messages.map(msg =>
            `[${msg.timestamp.toLocaleTimeString()}] ${msg.isUser ? 'User' : 'Assistant'}: ${msg.text}`
        ).join('\n');

        const blob = new Blob([transcript], { type: 'text/plain' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `transcript-${new Date().toISOString().split('T')[0]}.txt`;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
    };

    const formatTimestamp = (timestamp: Date) => {
        return timestamp.toLocaleTimeString([], {
            hour: '2-digit',
            minute: '2-digit',
            second: '2-digit'
        });
    };

    const getConfidenceColor = (confidence?: number) => {
        if (!confidence) return '';
        if (confidence > 0.8) return 'text-green-600';
        if (confidence > 0.6) return 'text-yellow-600';
        return 'text-red-600';
    };

    return (
        <Card className={className}>
            <CardHeader>
                <div className="flex items-center justify-between">
                    <div>
                        <CardTitle className="flex items-center gap-2">
                            <MessageSquare className="h-5 w-5" />
                            Live Transcript
                        </CardTitle>
                        <CardDescription>
                            Real-time conversation transcript
                        </CardDescription>
                    </div>

                    <div className="flex items-center gap-2">
                        <Label htmlFor="transcript-enabled" className="text-sm">
                            Enable
                        </Label>
                        <Switch
                            id="transcript-enabled"
                            checked={isEnabled}
                            onCheckedChange={setIsEnabled}
                        />
                    </div>
                </div>
            </CardHeader>

            <CardContent className="space-y-2">
                {/* Controls */}
                <div className="flex items-center justify-between">
                    <div className="flex items-center gap-2">
                        <Label htmlFor="auto-scroll" className="text-sm">
                            Auto-scroll
                        </Label>
                        <Switch
                            id="auto-scroll"
                            checked={isAutoScroll}
                            onCheckedChange={setIsAutoScroll}
                        />
                    </div>

                    <div className="flex gap-2">
                        <Button
                            onClick={exportTranscript}
                            variant="outline"
                            size="sm"
                            disabled={messages.length === 0}
                        >
                            <Download className="h-4 w-4 mr-1" />
                            Export
                        </Button>

                        <Button
                            onClick={clearMessages}
                            variant="outline"
                            size="sm"
                            disabled={messages.length === 0}
                        >
                            <Trash2 className="h-4 w-4 mr-1" />
                            Clear
                        </Button>
                    </div>
                </div>

                {/* Messages */}
                <div className="h-48 overflow-y-auto border rounded-lg p-2 space-y-2 bg-muted/20">
                    {messages.length === 0 ? (
                        <div className="flex items-center justify-center h-full text-muted-foreground">
                            <div className="text-center">
                                <MessageSquare className="h-8 w-8 mx-auto mb-2 opacity-50" />
                                <p>No messages yet</p>
                                <p className="text-sm">Start a conversation to see the transcript</p>
                            </div>
                        </div>
                    ) : (
                        messages.map((message) => (
                            <div
                                key={message.id}
                                className={`flex gap-3 p-3 rounded-lg ${message.isUser
                                    ? 'bg-primary/10 border-l-4 border-primary'
                                    : 'bg-secondary/50 border-l-4 border-secondary'
                                    }`}
                            >
                                <div className="flex-shrink-0">
                                    {message.isUser ? (
                                        <User className="h-5 w-5 text-primary" />
                                    ) : (
                                        <Bot className="h-5 w-5 text-secondary-foreground" />
                                    )}
                                </div>

                                <div className="flex-1 min-w-0">
                                    <div className="flex items-center gap-2 mb-1">
                                        <span className="font-medium text-sm">
                                            {message.isUser ? 'You' : 'Assistant'}
                                        </span>
                                        <div className="flex items-center gap-1 text-xs text-muted-foreground">
                                            <Clock className="h-3 w-3" />
                                            {formatTimestamp(message.timestamp)}
                                        </div>
                                        {message.confidence && (
                                            <span className={`text-xs ${getConfidenceColor(message.confidence)}`}>
                                                ({Math.round(message.confidence * 100)}%)
                                            </span>
                                        )}
                                    </div>

                                    <p className="text-sm break-words">
                                        {message.text}
                                    </p>
                                </div>
                            </div>
                        ))
                    )}
                    <div ref={messagesEndRef} />
                </div>

                {/* Stats */}
                {messages.length > 0 && (
                    <div className="text-xs text-muted-foreground text-center">
                        {messages.length} message{messages.length !== 1 ? 's' : ''} •
                        {messages.filter(m => m.isUser).length} from you •
                        {messages.filter(m => !m.isUser).length} from assistant
                    </div>
                )}
            </CardContent>
        </Card>
    );
}
